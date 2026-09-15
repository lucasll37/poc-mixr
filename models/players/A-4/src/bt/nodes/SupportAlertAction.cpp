#include "bt/nodes/SupportAlertAction.hpp"

#include "domain/geometry.hpp"
#include "bt/DecisionContext.hpp"

namespace mixr {
namespace models {
namespace xA_4 {
namespace bt_nodes {

SupportAlertAction::SupportAlertAction(const std::string& name, const BT::NodeConfiguration& config,
                                       const NodeContext& context)
   : BT::SyncActionNode(name, config), context_(context)
{
}

BT::NodeStatus SupportAlertAction::tick()
{
   if (context_.behavior == nullptr) return BT::NodeStatus::FAILURE;

   const auto& snap = context_.behavior->snapshot();
   if (!snap.hasAlert) return BT::NodeStatus::FAILURE;

   // Reacao ao evento de OUTRO player: voa para a posicao que o alerta
   // trouxe. O alerta nao mandou fazer isso -- ele so disse onde esta o
   // intruso; a decisao de apoiar e desta aeronave.
   domain::FlightCommand cmd;
   cmd.headingDeg = domain::headingToDeg(snap.northM, snap.eastM,
                                         snap.alertNorthM, snap.alertEastM);
   // snap.alertAltitudeM e a altitude absoluta do contato reportado por
   // outro player via datalink -- vem de fora da propria aeronave, sem
   // garantia nenhuma de respeitar o terreno; sem o clamp, um intruso
   // reportado voando baixo faria o suporte descer ate a mesma altitude
   // sem validacao.
   cmd.altitudeM = context_.behavior->clampAltitudeToTerrain(snap.alertAltitudeM);
   cmd.speedKts = context_.behavior->getSupportSpeedKts();

   context_.behavior->decision().take(cmd, "SUPPORT");
   return BT::NodeStatus::SUCCESS;
}

} // namespace bt_nodes
} // namespace xA_4
} // namespace models
} // namespace mixr
