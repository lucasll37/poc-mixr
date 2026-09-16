#include "bt/nodes/OnnxScoreCondition.hpp"

#include "bt/DecisionContext.hpp"
#include "bt/ObservationSchema.hpp"

#include "xinfer/Infer.hpp"
#include "xlog/Log.hpp"

#include <array>
#include <vector>

namespace mixr {
namespace models {
namespace xA_4 {
namespace bt_nodes {

OnnxScoreCondition::OnnxScoreCondition(const std::string& name, const BT::NodeConfiguration& config,
                                       const NodeContext& context)
   : BT::ConditionNode(name, config), context_(context)
{
}

BT::PortsList OnnxScoreCondition::providedPorts()
{
   return {
      BT::InputPort<std::string>("model", "",
                                 "caminho do .onnx (entrada = tamanho do schema resolvido)"),
      BT::InputPort<double>("threshold", 0.5, "limiar de comparacao"),
      BT::InputPort<int>("index", 0, "qual saida do modelo comparar"),
      BT::InputPort<bool>("above", true, "true: SUCCESS se saida > limiar; false: se saida < limiar"),
      BT::InputPort<std::string>("schema", "classic28",
                                 "campos da observacao, e ordem: 'classic28' (default), "
                                 "'all', ou lista ad-hoc separada por espaco"),
   };
}

BT::NodeStatus OnnxScoreCondition::tick()
{
   if (context_.behavior == nullptr) return BT::NodeStatus::FAILURE;

   // UMA tentativa de carga, e so. Mesmo molde do 'treeBuilt' de
   // ubf/BtBehavior: um caminho errado no XML nao pode fazer o no tentar
   // reabrir o arquivo 50 vezes por segundo.
   if (!tentouAbrir_) {
      tentouAbrir_ = true;

      std::string schemaValor{"classic28"};
      if (const BT::Optional<std::string> in{getInput<std::string>("schema")}) schemaValor = in.value();
      try {
         bound_ = xrlbridge::bind<domain::WorldView>(resolveObservationSchema(schemaValor),
                                                      domain::worldViewFieldRegistry());
      } catch (const xrlbridge::SchemaError& ex) {
         LOG(ERROR) << "[OnnxScore] " << ex.what();
         return BT::NodeStatus::FAILURE;
      }

      const BT::Optional<std::string> caminho{getInput<std::string>("model")};
      if (!caminho || caminho.value().empty()) {
         LOG(ERROR) << "[OnnxScore] porta 'model' ausente ou vazia no XML da arvore";
      } else {
         modelId_ = mixr::xinfer::open(caminho.value());
         if (modelId_ != 0) {
            int nIn{}, nOut{};
            const int nEsperado{static_cast<int>(bound_.resolved.size())};
            if (mixr::xinfer::shape(modelId_, nIn, nOut) && nIn != nEsperado) {
               LOG(ERROR) << "[OnnxScore] '" << caminho.value() << "' espera " << nIn
                          << " entradas, mas o schema '" << bound_.schema.name << "' tem "
                          << nEsperado;
               modelId_ = 0;
            } else if (modelId_ != 0) {
               // Contagem batendo nao basta: dois .onnx do mesmo tamanho
               // podem esperar campos DIFERENTES -- checado por IDENTIDADE
               // quando o .onnx traz a metadata (xinfer::fields()); sem ela,
               // so a checagem de contagem acima vale, como sempre.
               std::vector<std::string> declarados;
               if (mixr::xinfer::fields(modelId_, declarados)) {
                  std::vector<std::string> esperados;
                  esperados.reserve(bound_.resolved.size());
                  for (const auto* const decl : bound_.resolved) esperados.push_back(decl->name);
                  if (declarados != esperados) {
                     LOG(ERROR) << "[OnnxScore] '" << caminho.value()
                                << "' foi exportado para outros campos (ou outra ordem) que o "
                                << "schema '" << bound_.schema.name
                                << "' resolveu -- reexporte ou corrija a porta 'schema'";
                     modelId_ = 0;
                  }
               }
            }
         }
      }
   }
   if (modelId_ == 0) return BT::NodeStatus::FAILURE;

   const domain::WorldView& snap{context_.behavior->snapshot()};
   std::vector<float> entrada(bound_.resolved.size());
   xrlbridge::pack(bound_, snap, entrada.data());

   std::array<float, 16> saida{};
   const int escritos{mixr::xinfer::run(modelId_, entrada.data(),
                                        static_cast<int>(entrada.size()),
                                        saida.data(), static_cast<int>(saida.size()))};
   if (escritos <= 0) return BT::NodeStatus::FAILURE;

   int indice{};
   if (const BT::Optional<int> in{getInput<int>("index")}) indice = in.value();
   if (indice < 0 || indice >= escritos) return BT::NodeStatus::FAILURE;

   double limiar{0.5};
   if (const BT::Optional<double> in{getInput<double>("threshold")}) limiar = in.value();

   bool acima{true};
   if (const BT::Optional<bool> in{getInput<bool>("above")}) acima = in.value();

   const double valor{static_cast<double>(saida[static_cast<std::size_t>(indice)])};
   const bool passou{acima ? (valor > limiar) : (valor < limiar)};
   return passou ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
}

} // namespace bt_nodes
} // namespace xA_4
} // namespace models
} // namespace mixr
