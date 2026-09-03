#include "bt/nodes/OnnxScoreCondition.hpp"

#include "bt/DecisionContext.hpp"

#include "xinfer/Infer.hpp"
#include "xlog/Log.hpp"
#include "xrlbridge/ObservationFields.hpp"

#include <array>

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
                                 "caminho do .onnx (entrada float32[1,28], ver ObservationFields.hpp)"),
      BT::InputPort<double>("threshold", 0.5, "limiar de comparacao"),
      BT::InputPort<int>("index", 0, "qual saida do modelo comparar"),
      BT::InputPort<bool>("above", true, "true: SUCCESS se saida > limiar; false: se saida < limiar"),
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
      const BT::Optional<std::string> caminho{getInput<std::string>("model")};
      if (!caminho || caminho.value().empty()) {
         LOG(ERROR) << "[OnnxScore] porta 'model' ausente ou vazia no XML da arvore";
      } else {
         modelId_ = mixr::xinfer::open(caminho.value());
         if (modelId_ != 0) {
            int nIn{}, nOut{};
            if (mixr::xinfer::shape(modelId_, nIn, nOut) && nIn != XRLBRIDGE_OBSERVATION_SIZE) {
               LOG(ERROR) << "[OnnxScore] '" << caminho.value() << "' espera " << nIn
                          << " entradas, mas a observacao canonica tem "
                          << XRLBRIDGE_OBSERVATION_SIZE;
               modelId_ = 0;
            }
         }
      }
   }
   if (modelId_ == 0) return BT::NodeStatus::FAILURE;

   // A observacao, na ORDEM CANONICA -- a mesma macro que o treino usa. Note
   // que ela e expandida aqui contra domain::WorldView, e em
   // shared/xrlbridge/RLBridge.cpp contra xrlbridge::Observation: um nome que
   // divergir entre as duas structs nao compila.
   const domain::WorldView& snap{context_.behavior->snapshot()};
   std::array<float, XRLBRIDGE_OBSERVATION_SIZE> entrada{};
   {
      int i{};
#define XRLBRIDGE_F(nome) entrada[i++] = static_cast<float>(snap.nome);
#define XRLBRIDGE_B(nome) entrada[i++] = snap.nome ? 1.0F : 0.0F;
      XRLBRIDGE_OBSERVATION_FIELDS
#undef XRLBRIDGE_F
#undef XRLBRIDGE_B
   }

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
