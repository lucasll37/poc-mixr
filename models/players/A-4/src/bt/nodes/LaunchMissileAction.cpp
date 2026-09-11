#include "bt/nodes/LaunchMissileAction.hpp"

#include "bt/DecisionContext.hpp"

namespace bt_nodes {

LaunchMissileAction::LaunchMissileAction(const std::string& name,
                                         const BT::NodeConfiguration& config,
                                         const NodeContext& context)
   : BT::SyncActionNode(name, config), context_(context)
{
}

//------------------------------------------------------------------------------
// So marca o PEDIDO -- o comando de voo mantem rumo/altitude/velocidade
// atuais (disparar nao e motivo pra mudar de trajetoria; o disparo em si
// acontece na atuacao, ver ubf/FlightAction.cpp). Mesmo piso anti-CFIT que
// RTB/SUPPORT/PATROL ja respeitam fora do ramo de evasao (ver
// bt/DecisionContext.hpp::clampAltitudeToTerrain()).
//
// So' dispara UMA vez por engajamento sem precisar de estado proprio aqui:
// depois do primeiro disparo, StoresMgr::available() cai (um cenario com
// um so' missil no cabide vai a zero), snapshot().weaponReady vira false no
// PROXIMO frame, e LaunchEnvelopeCondition passa a falhar sozinho -- a
// mesma latencia de um frame entre decisao e percepcao que o resto deste
// modelo ja tem (ver o comentario de domain::ThreatPolicy sobre
// contactLive()/engaged()).
//------------------------------------------------------------------------------
BT::NodeStatus LaunchMissileAction::tick()
{
   if (context_.behavior == nullptr) return BT::NodeStatus::FAILURE;

   const auto& snap = context_.behavior->snapshot();
   FlightDecision& decision{context_.behavior->decision()};

   domain::FlightCommand cmd;
   cmd.headingDeg = snap.headingDeg;
   cmd.altitudeM = context_.behavior->clampAltitudeToTerrain(snap.altitudeM);
   cmd.speedKts = snap.speedKts;
   decision.take(cmd, "LAUNCH");

   decision.launchRequested = true;
   decision.launchTargetName = snap.contactName;

   return BT::NodeStatus::SUCCESS;
}

} // namespace bt_nodes
