#include "app/HttpServer.hpp"

#include "app/ScenarioUpload.hpp"
#include "app/Subprocess.hpp"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <atomic>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <unistd.h>

namespace app {

namespace {

namespace fs = std::filesystem;

//------------------------------------------------------------------------------
// Semaforo contador minimo -- so tryAcquire/release, sem espera bloqueante:
// quando esgotado, a rota devolve 503 na hora em vez de enfileirar. O
// repositorio compila em C++17 (ver meson.build raiz); std::counting_semaphore
// e C++20, dai este substituto de poucas linhas.
//------------------------------------------------------------------------------
class Semaphore
{
public:
   explicit Semaphore(const int count) : count_{count} {}

   bool tryAcquire()
   {
      std::lock_guard<std::mutex> lock{mutex_};
      if (count_ <= 0) return false;
      --count_;
      return true;
   }

   void release()
   {
      std::lock_guard<std::mutex> lock{mutex_};
      ++count_;
   }

private:
   std::mutex mutex_;
   int count_;
};

class SemaphoreGuard
{
public:
   explicit SemaphoreGuard(Semaphore& sem) : sem_{sem} {}
   ~SemaphoreGuard() { sem_.release(); }
private:
   Semaphore& sem_;
};

// Mesma tecnica de app/Respawn.hpp (documentada no CLAUDE.md): resolver o
// PROPRIO caminho por '/proc/self/exe', nunca por argv[0] (que pode vir
// relativo). O irmao 'sim-runner' mora no MESMO diretorio de saida do
// meson que 'server' -- os dois executable() vem do mesmo meson.build.
std::string resolveDefaultRunnerPath()
{
   char buf[4096];
   const ssize_t n{readlink("/proc/self/exe", buf, sizeof(buf) - 1)};
   if (n <= 0) return "./sim-runner";
   buf[n] = '\0';
   const fs::path exe{buf};
   return (exe.parent_path() / "sim-runner").string();
}

std::string lastNonEmptyLine(const std::string& text)
{
   std::size_t end{text.size()};
   while (end > 0) {
      std::size_t start{text.rfind('\n', end - 1)};
      if (start == std::string::npos) start = 0; else start += 1;
      const std::string line{text.substr(start, end - start)};
      bool blank{true};
      for (const char c : line) { if (!std::isspace(static_cast<unsigned char>(c))) { blank = false; break; } }
      if (!blank) return line;
      if (start == 0) break;
      end = start - 1;
   }
   return {};
}

std::string truncate(const std::string& text, const std::size_t maxLen)
{
   if (text.size() <= maxLen) return text;
   return text.substr(0, maxLen) + "... (truncado)";
}

nlohmann::json errorBody(const std::string& error, const std::vector<std::string>& details = {})
{
   nlohmann::json j;
   j["error"] = error;
   if (!details.empty()) j["details"] = details;
   return j;
}

void sendJson(httplib::Response& res, const int status, const nlohmann::json& body)
{
   res.status = status;
   res.set_content(body.dump(), "application/json");
}

std::atomic<long> gRunCounter{0};

// Diretorio de trabalho POR REQUISICAO -- cada uma escreve so o proprio
// arquivo, entao requisicoes concorrentes nao colidem (ver README.md).
fs::path makeRunDir()
{
   const long id{gRunCounter.fetch_add(1)};
   const fs::path dir{fs::path{"./src/server/data/runs"} /
      (std::to_string(::getpid()) + "-" + std::to_string(id))};
   fs::create_directories(dir);
   return dir;
}

} // namespace

int runHttpServer(const ServerOptions& opts)
{
   const std::string runnerPath{!opts.runnerPath.empty() ? opts.runnerPath : resolveDefaultRunnerPath()};
   std::cerr << "[server] sim-runner: " << runnerPath << std::endl;

   Semaphore concurrencyLimit{opts.maxConcurrentSims};

   httplib::Server svr;

   // cpp-httplib chama isto para TODO status >= 400 -- inclusive quando a
   // propria rota ja escreveu um corpo de erro de proposito (400 de
   // validacao, 422 de cenario malformado). Sem a guarda de 'res.body',
   // este handler SOBRESCREVIA esses corpos com "not_found" mesmo com o
   // status certo -- medido rodando: 400/422 saiam com status correto e
   // corpo errado. So preenche quando a rota nao deixou nada (404 de
   // caminho desconhecido, por exemplo).
   svr.set_error_handler([](const httplib::Request&, httplib::Response& res) {
      if (!res.body.empty()) return;
      sendJson(res, res.status, errorBody("not_found"));
   });
   svr.set_exception_handler([](const httplib::Request&, httplib::Response& res, const std::exception_ptr&) {
      sendJson(res, 500, errorBody("internal_error"));
   });

   svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
      nlohmann::json j; j["status"] = "ok";
      sendJson(res, 200, j);
   });

   svr.Post("/simulate", [&](const httplib::Request& req, httplib::Response& res) {
      std::vector<std::string> paramErrors;

      long frames{opts.defaultFrames};
      if (req.has_param("frames")) {
         try {
            frames = std::stol(req.get_param_value("frames"));
         } catch (...) {
            paramErrors.push_back("frames: nao e um inteiro valido");
         }
      }
      if (frames < 1 || frames > opts.maxFrames) {
         paramErrors.push_back("frames: deve estar entre 1 e " + std::to_string(opts.maxFrames));
      }

      bool hasThreads{req.has_param("threads")};
      int threads{};
      if (hasThreads) {
         try {
            threads = std::stoi(req.get_param_value("threads"));
         } catch (...) {
            paramErrors.push_back("threads: nao e um inteiro valido");
         }
         if (threads < 1 || threads > opts.maxThreads) {
            paramErrors.push_back("threads: deve estar entre 1 e " + std::to_string(opts.maxThreads));
         }
      }

      const UploadValidation upload{validateScenarioBody(req.body, opts.maxBodyBytes)};
      for (const auto& e : upload.errors) paramErrors.push_back(e);

      if (!paramErrors.empty()) {
         sendJson(res, 400, errorBody("validation_failed", paramErrors));
         return;
      }

      if (!concurrencyLimit.tryAcquire()) {
         sendJson(res, 503, errorBody("too_many_requests"));
         return;
      }
      SemaphoreGuard guard{concurrencyLimit};

      const fs::path runDir{makeRunDir()};
      const fs::path bodyPath{runDir / "scenario_body.epp"};
      std::error_code cleanupEc;

      {
         std::ofstream out{bodyPath};
         out << req.body;
      }

      std::vector<std::string> args{"-f", bodyPath.string(), "-frames", std::to_string(frames)};
      if (hasThreads) { args.push_back("-threads"); args.push_back(std::to_string(threads)); }

      const SubprocessResult result{runSubprocess(runnerPath, args, opts.subprocessTimeoutSec)};

      fs::remove_all(runDir, cleanupEc);

      if (result.timedOut) {
         sendJson(res, 504, errorBody("timeout"));
         return;
      }
      if (result.exitCode != 0) {
         // '=', NAO '{}' -- ver o comentario identico em TelemetryJson.cpp:
         // 'json body{errorBody(...)}' cairia no construtor de
         // initializer_list e embrulharia o objeto inteiro dentro de um
         // array de um elemento, quebrando o body["exitCode"] logo abaixo
         // (operator[] com chave string num array lanca type_error).
         nlohmann::json body = errorBody("scenario_build_failed");
         body["exitCode"] = result.exitCode;
         body["stderr"] = truncate(result.stderrText, 4096);
         sendJson(res, 422, body);
         return;
      }

      const std::string jsonLine{lastNonEmptyLine(result.stdoutText)};
      nlohmann::json telemetry;
      try {
         telemetry = nlohmann::json::parse(jsonLine);
      } catch (const std::exception&) {
         nlohmann::json body = errorBody("internal_error");   // ver o comentario acima
         body["message"] = "resposta do sim-runner nao e JSON valido";
         sendJson(res, 500, body);
         return;
      }

      sendJson(res, 200, telemetry);
   });

   std::cout << "[server] escutando em " << opts.bindAddress << ":" << opts.port << std::endl;
   const bool ok{svr.listen(opts.bindAddress, opts.port)};
   return ok ? 0 : 1;
}

} // namespace app
