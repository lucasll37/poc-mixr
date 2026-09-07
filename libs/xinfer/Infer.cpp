#include "xinfer/Infer.hpp"

#include "xlog/Log.hpp"

#include <onnxruntime_cxx_api.h>

#include <array>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

namespace mixr {
namespace xinfer {

namespace {

// Um Env por processo. O ONNX Runtime quer exatamente isso -- ele carrega o
// registro de kernels e o logger uma vez. Local a funcao para nascer na
// primeira carga, e nao numa ordem de inicializacao estatica que nao
// controlamos (este .so e alcancado tanto pelo executavel quanto por um
// plugin aberto com dlopen).
Ort::Env& env()
{
   static Ort::Env instancia{ORT_LOGGING_LEVEL_ERROR, "xinfer"};
   return instancia;
}

struct Model
{
   std::unique_ptr<Ort::Session> session;
   std::string inName;
   std::string outName;
   int nIn{};
   int nOut{};
};

// Um mutex so, e um cache minusculo (um modelo por politica -- hoje, um).
// 'byId' e indexado pelo ModelId: a posicao 0 fica vazia de proposito, para
// que o id 0 seja sempre invalido, e o vetor SO CRESCE, entao um id nunca
// muda de significado.
std::mutex g_mutex;
std::vector<std::shared_ptr<Model>> g_byId{nullptr};
std::map<std::string, ModelId> g_porCaminho;

} // namespace

ModelId open(const std::string& path)
{
   if (path.empty()) {
      LOG(ERROR) << "[xinfer] open() com caminho vazio";
      return 0;
   }

   std::lock_guard<std::mutex> lock(g_mutex);

   const auto ja = g_porCaminho.find(path);
   if (ja != g_porCaminho.end()) return ja->second;

   try {
      Ort::SessionOptions opcoes;
      // Ver o cabecalho: determinismo E velocidade. Nao mexer sem medir os
      // dois lados.
      opcoes.SetIntraOpNumThreads(1);
      opcoes.SetInterOpNumThreads(1);
      opcoes.SetExecutionMode(ORT_SEQUENTIAL);
      opcoes.DisableCpuMemArena();
      opcoes.DisableMemPattern();

      auto modelo = std::make_shared<Model>();
      modelo->session = std::make_unique<Ort::Session>(env(), path.c_str(), opcoes);

      Ort::AllocatorWithDefaultOptions alocador;
      modelo->inName  = modelo->session->GetInputNameAllocated(0, alocador).get();
      modelo->outName = modelo->session->GetOutputNameAllocated(0, alocador).get();

      const auto formaIn  = modelo->session->GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
      const auto formaOut = modelo->session->GetOutputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
      if (formaIn.empty() || formaOut.empty()) {
         LOG(ERROR) << "[xinfer] '" << path << "' tem entrada ou saida sem dimensao";
         return 0;
      }
      modelo->nIn  = static_cast<int>(formaIn.back());
      modelo->nOut = static_cast<int>(formaOut.back());
      if (modelo->nIn <= 0 || modelo->nOut <= 0) {
         LOG(ERROR) << "[xinfer] '" << path << "' tem dimensao dinamica na ultima posicao"
                    << " (in=" << modelo->nIn << " out=" << modelo->nOut << ")";
         return 0;
      }

      g_byId.push_back(modelo);
      const ModelId id{static_cast<ModelId>(g_byId.size() - 1)};
      g_porCaminho[path] = id;
      return id;

   } catch (const std::exception& ex) {
      // Degrada, nao aborta -- quem chamou recebe 0 e decide o que fazer.
      LOG(ERROR) << "[xinfer] falha ao abrir '" << path << "': " << ex.what();
      return 0;
   }
}

bool shape(const ModelId id, int& nIn, int& nOut)
{
   std::lock_guard<std::mutex> lock(g_mutex);
   if (id <= 0 || static_cast<std::size_t>(id) >= g_byId.size()) return false;
   const auto& modelo = g_byId[static_cast<std::size_t>(id)];
   if (!modelo) return false;
   nIn = modelo->nIn;
   nOut = modelo->nOut;
   return true;
}

int run(const ModelId id, const float* const in, const int nIn, float* const out, const int nOut)
{
   if (in == nullptr || out == nullptr || nIn <= 0 || nOut <= 0) return -1;

   // O mutex cobre SO a busca do ponteiro; a inferencia roda fora dele.
   std::shared_ptr<Model> modelo;
   {
      std::lock_guard<std::mutex> lock(g_mutex);
      if (id <= 0 || static_cast<std::size_t>(id) >= g_byId.size()) return -1;
      modelo = g_byId[static_cast<std::size_t>(id)];
   }
   if (!modelo) return -1;
   if (nIn != modelo->nIn) return -3;   // forma errada: erro do chamador, nao do modelo

   try {
      const std::array<std::int64_t, 2> forma{1, static_cast<std::int64_t>(nIn)};
      auto memoria = Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU);
      auto tensor = Ort::Value::CreateTensor<float>(
         memoria, const_cast<float*>(in), static_cast<std::size_t>(nIn),
         forma.data(), forma.size());

      const char* const entradas[]{modelo->inName.c_str()};
      const char* const saidas[]{modelo->outName.c_str()};

      auto resultado = modelo->session->Run(Ort::RunOptions{nullptr},
                                            entradas, &tensor, 1, saidas, 1);

      const int quantos{(modelo->nOut < nOut) ? modelo->nOut : nOut};
      std::memcpy(out, resultado[0].GetTensorData<float>(),
                  sizeof(float) * static_cast<std::size_t>(quantos));
      return quantos;

   } catch (const std::exception& ex) {
      LOG(ERROR) << "[xinfer] falha na inferencia: " << ex.what();
      return -2;
   }
}

} // namespace xinfer
} // namespace mixr
