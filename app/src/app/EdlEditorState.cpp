#include "app/EdlEditorState.hpp"

#include <climits>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <sys/wait.h>
#include <unistd.h>

namespace app {

namespace {

// Aspas simples, com o escape padrao de shell para uma aspa simples dentro
// de um trecho ja entre aspas simples -- os caminhos aqui sao sempre os que
// este proprio processo gerou, nunca texto de usuario, mas a citacao correta
// custa nada e evita depender dessa premissa se um dia deixar de valer.
std::string shellQuote(const std::string& s)
{
   std::string out{"'"};
   for (const char c : s) {
      if (c == '\'') out += "'\"'\"'";
      else out += c;
   }
   out += "'";
   return out;
}

} // namespace

const std::string& editedScenarioPath()
{
   static const std::string path{"./app/data/edl_editor/edited.edl"};
   return path;
}

std::string readEdlFileOrEmpty(const std::string& path)
{
   std::ifstream in(path);
   if (!in.good()) return {};
   std::ostringstream buf;
   buf << in.rdbuf();
   return buf.str();
}

bool writeEdlFile(const std::string& path, const std::string& text)
{
   const std::filesystem::path p{path};
   if (p.has_parent_path()) {
      std::error_code ec;
      std::filesystem::create_directories(p.parent_path(), ec);
   }
   std::ofstream out(path, std::ios::trunc);
   if (!out.good()) return false;
   out << text;
   return out.good();
}

EdlValidationResult runEdlCheck(const std::string& edlcheckBinaryPath, const std::string& edlFilePath)
{
   EdlValidationResult result;

   const std::string cmd{shellQuote(edlcheckBinaryPath) + " " + shellQuote(edlFilePath) + " 2>&1"};
   FILE* const pipe{popen(cmd.c_str(), "r")};
   if (pipe == nullptr) {
      result.message = "nao consegui executar '" + edlcheckBinaryPath + "'";
      return result;
   }

   std::ostringstream out;
   char buf[256];
   while (fgets(buf, sizeof(buf), pipe) != nullptr) out << buf;
   const int rc{pclose(pipe)};

   result.message = out.str();
   while (!result.message.empty()
         && (result.message.back() == '\n' || result.message.back() == '\r')) {
      result.message.pop_back();
   }
   result.ok = (rc != -1) && WIFEXITED(rc) && WEXITSTATUS(rc) == 0;
   if (result.message.empty()) {
      result.message = result.ok ? "OK" : "edlcheck terminou com erro, sem mensagem";
   }
   return result;
}

std::string edlcheckSiblingPath()
{
   char selfPath[PATH_MAX]{};
   const ssize_t n{readlink("/proc/self/exe", selfPath, sizeof(selfPath) - 1)};
   if (n <= 0) return "edlcheck";   // fallback: espera achar no PATH
   selfPath[n] = '\0';

   const std::string self{selfPath};
   const std::size_t slash{self.find_last_of('/')};
   if (slash == std::string::npos) return "edlcheck";
   return self.substr(0, slash + 1) + "edlcheck";
}

} // namespace app
