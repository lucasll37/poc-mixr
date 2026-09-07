#include "bt/nodes/PyDecideAction.hpp"

#include "bt/DecisionContext.hpp"

#include "xlog/Log.hpp"
#include "xpyembed/PyEmbed.hpp"
#include "xrlbridge/ObservationFields.hpp"

#include <array>
#include <atomic>

namespace bt_nodes {

namespace {
// Um id por INSTANCIA do no -- ver o "porque" no cabecalho. Atomico porque as
// arvores das quatro aeronaves podem ser construidas em threads diferentes do
// pool de tempo critico.
std::atomic<int> g_proximaInstancia{1};
}

PyDecideAction::PyDecideAction(const std::string& name, const BT::NodeConfiguration& config,
                               const NodeContext& context)
   : BT::SyncActionNode(name, config), context_(context),
     instanciaId_(g_proximaInstancia.fetch_add(1))
{
}

BT::PortsList PyDecideAction::providedPorts()
{
   return {
      BT::InputPort<std::string>("script", "", "caminho do .py que define decide(obs)"),
      BT::InputPort<std::string>("label", "PY", "rotulo no dump e no quadro"),
   };
}

BT::NodeStatus PyDecideAction::tick()
{
   if (context_.behavior == nullptr) return BT::NodeStatus::FAILURE;

   if (!tentouCarregar_) {
      tentouCarregar_ = true;
      const BT::Optional<std::string> caminho{getInput<std::string>("script")};
      if (!caminho || caminho.value().empty()) {
         LOG(ERROR) << "[PyDecide] porta 'script' ausente ou vazia no XML da arvore";
      } else if (!mixr::xpyembed::isAvailable()) {
         LOG(WARNING) << "[PyDecide] sem interpretador Python -- o no fica inerte";
      } else {
         scriptId_ = mixr::xpyembed::loadScript(caminho.value());
      }
   }
   if (scriptId_ == 0) return BT::NodeStatus::FAILURE;

   // A observacao na ordem canonica -- a MESMA macro do .onnx e do treino.
   const domain::WorldView& snap{context_.behavior->snapshot()};
   std::array<double, XRLBRIDGE_OBSERVATION_SIZE> entrada{};
   {
      int i{};
#define XRLBRIDGE_F(nome) entrada[i++] = static_cast<double>(snap.nome);
#define XRLBRIDGE_B(nome) entrada[i++] = snap.nome ? 1.0 : 0.0;
      XRLBRIDGE_OBSERVATION_FIELDS
#undef XRLBRIDGE_F
#undef XRLBRIDGE_B
   }

   std::array<double, XRLBRIDGE_ACTION_SIZE> saida{};
   if (!mixr::xpyembed::decide(scriptId_, instanciaId_,
                               entrada.data(), static_cast<int>(entrada.size()),
                               saida.data(), static_cast<int>(saida.size()))) {
      return BT::NodeStatus::FAILURE;
   }

   domain::FlightCommand cmd;
   cmd.headingDeg = saida[0];
   cmd.altitudeM = saida[1];
   cmd.speedKts = saida[2];

   std::string rotulo{"PY"};
   if (const BT::Optional<std::string> in{getInput<std::string>("label")}) rotulo = in.value();

   context_.behavior->decision().take(cmd, rotulo);
   return BT::NodeStatus::SUCCESS;
}

} // namespace bt_nodes
