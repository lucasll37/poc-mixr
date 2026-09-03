#include "bt/nodes/OnnxPolicyAction.hpp"

#include "bt/DecisionContext.hpp"

#include "xinfer/Infer.hpp"
#include "xlog/Log.hpp"
#include "xrlbridge/ObservationFields.hpp"
#include "xrlbridge/RLBridge.hpp"

#include <array>

namespace bt_nodes {

OnnxPolicyAction::OnnxPolicyAction(const std::string& name, const BT::NodeConfiguration& config,
                                   const NodeContext& context)
   : BT::SyncActionNode(name, config), context_(context)
{
}

BT::PortsList OnnxPolicyAction::providedPorts()
{
   return {
      BT::InputPort<std::string>("model", "", "caminho do .onnx da politica"),
      BT::InputPort<bool>("normalized", true,
                          "true: saida em [-1,1], desnormalizada aqui (export do SB3)"),
      BT::InputPort<std::string>("label", "ONNX", "rotulo no dump e no quadro"),
   };
}

BT::NodeStatus OnnxPolicyAction::tick()
{
   if (context_.behavior == nullptr) return BT::NodeStatus::FAILURE;

   if (!tentouAbrir_) {
      tentouAbrir_ = true;
      const BT::Optional<std::string> caminho{getInput<std::string>("model")};
      if (!caminho || caminho.value().empty()) {
         LOG(ERROR) << "[OnnxPolicy] porta 'model' ausente ou vazia no XML da arvore";
      } else {
         modelId_ = mixr::xinfer::open(caminho.value());
         if (modelId_ != 0) {
            int nIn{}, nOut{};
            if (mixr::xinfer::shape(modelId_, nIn, nOut)) {
               // A forma e contrato, nao sugestao: 28 entrada, 3 saida. Um
               // .onnx com outra forma foi treinado contra outra observacao
               // ou outra acao, e comandar com ele seria pior que nao
               // comandar.
               if (nIn != XRLBRIDGE_OBSERVATION_SIZE || nOut != XRLBRIDGE_ACTION_SIZE) {
                  LOG(ERROR) << "[OnnxPolicy] '" << caminho.value() << "' tem forma "
                             << nIn << "->" << nOut << ", mas o contrato e "
                             << XRLBRIDGE_OBSERVATION_SIZE << "->" << XRLBRIDGE_ACTION_SIZE
                             << " (ver xrlbridge/ObservationFields.hpp)";
                  modelId_ = 0;
               }
            }
         }
      }
   }
   if (modelId_ == 0) return BT::NodeStatus::FAILURE;

   // A observacao na ordem canonica -- a MESMA macro do treino.
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

   std::array<float, XRLBRIDGE_ACTION_SIZE> saida{};
   const int escritos{mixr::xinfer::run(modelId_, entrada.data(),
                                        static_cast<int>(entrada.size()),
                                        saida.data(), static_cast<int>(saida.size()))};
   if (escritos != XRLBRIDGE_ACTION_SIZE) return BT::NodeStatus::FAILURE;

   bool normalizada{true};
   if (const BT::Optional<bool> in{getInput<bool>("normalized")}) normalizada = in.value();

   domain::FlightCommand cmd;
   if (normalizada) {
      // Uma unica implementacao da desnormalizacao, em shared/xrlbridge, com
      // os mesmos limites que o lado Python usa para montar o action_space.
      const mixr::xrlbridge::Command c{mixr::xrlbridge::unscaleCommand(saida.data())};
      cmd.headingDeg = c.headingDeg;
      cmd.altitudeM = c.altitudeM;
      cmd.speedKts = c.speedKts;
   } else {
      cmd.headingDeg = static_cast<double>(saida[0]);
      cmd.altitudeM = static_cast<double>(saida[1]);
      cmd.speedKts = static_cast<double>(saida[2]);
   }

   std::string rotulo{"ONNX"};
   if (const BT::Optional<std::string> in{getInput<std::string>("label")}) rotulo = in.value();

   context_.behavior->decision().take(cmd, rotulo);
   return BT::NodeStatus::SUCCESS;
}

} // namespace bt_nodes
