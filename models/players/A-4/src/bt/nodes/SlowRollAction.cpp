#include "bt/nodes/SlowRollAction.hpp"

#include "bt/DecisionContext.hpp"

namespace bt_nodes {

SlowRollAction::SlowRollAction(const std::string& name, const BT::NodeConfiguration& config,
                               const NodeContext& context)
   : BT::SyncActionNode(name, config), context_(context)
{
}

BT::PortsList SlowRollAction::providedPorts()
{
   return { BT::InputPort<std::string>("label", "ROLL",
                                       "rotulo no dump e no quadro durante a manobra") };
}

//------------------------------------------------------------------------------
// tick() -- avanca o plano e, SO enquanto a manobra esta em curso, comanda o
// aileron.
//
// O comando de rolagem nao passa pelo rumo: 'rollOverride' liga o caminho de
// stick da atuacao (ver domain/FlightCommand.hpp e ubf/FlightAction.cpp), que
// desliga o heading hold do Autopilot -- o unico jeito de o
// setControlStickRollInput() de fato alcancar o dynamics model.
//
// Altitude e velocidade CONTINUAM sendo comandadas, e a altitude continua
// passando pelo piso anti-CFIT: liberar um eixo nao e motivo para abrir mao
// da rede de seguranca dos outros dois (ver bt/DecisionContext.hpp,
// clampAltitudeToTerrain()).
//------------------------------------------------------------------------------
BT::NodeStatus SlowRollAction::tick()
{
   if (context_.behavior == nullptr) return BT::NodeStatus::FAILURE;

   auto& plan = context_.behavior->aerobaticPlan();
   const auto& view = context_.behavior->snapshot();

   // O plano precisa ser avancado TODO tick em que o no e' visitado -- e' o
   // 'dt' dele que faz a contagem regressiva ate a proxima manobra correr.
   if (!plan.update(context_.behavior->getFrameDt(), view.rollDeg)) {
      return BT::NodeStatus::FAILURE;
   }

   std::string label{"ROLL"};
   const BT::Optional<std::string> input{getInput<std::string>("label")};
   if (input) label = input.value();

   domain::FlightCommand cmd;
   cmd.rollOverride = true;
   cmd.rollStick = plan.stick();
   cmd.headingDeg = view.headingDeg;   // ignorado pela atuacao, mas mantem o dump coerente
   cmd.altitudeM = context_.behavior->clampAltitudeToTerrain(view.altitudeM);
   cmd.speedKts = view.speedKts;

   context_.behavior->decision().take(cmd, label);
   return BT::NodeStatus::SUCCESS;
}

} // namespace bt_nodes
