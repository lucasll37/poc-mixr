#include "bt/nodes/ReportAndEvadeAction.hpp"

#include "domain/ThreatPolicy.hpp"
#include "ubf/BtBehavior.hpp"

namespace bt_nodes {

ReportAndEvadeAction::ReportAndEvadeAction(const std::string& name,
                                           const BT::NodeConfiguration& config,
                                           const NodeContext& context)
   : BT::SyncActionNode(name, config), context_(context)
{
}

//------------------------------------------------------------------------------
// O no NAO calcula a manobra: ele so entrega o comando que a politica fixou
// na entrada da evasao (ver domain/ThreatPolicy.hpp -- o alvo e calculado uma
// vez e mantido, para o piloto automatico ter para onde convergir).
//
// Dois rotulos, porque sao dois estados diferentes e vale ve-los no status:
//    EVADE  -- quebrando COM o intruso na tela
//    BREAK  -- terminando a quebra no arrasto da histerese, ja sem pista
//------------------------------------------------------------------------------
BT::NodeStatus ReportAndEvadeAction::tick()
{
   if (context_.behavior == nullptr) return BT::NodeStatus::FAILURE;

   const domain::ThreatPolicy& policy{context_.behavior->threatPolicy()};
   if (!policy.engaged()) return BT::NodeStatus::FAILURE;

   const auto& snap = context_.behavior->snapshot();
   FlightDecision& decision{context_.behavior->decision()};

   decision.take(policy.command(), policy.contactLive() ? "EVADE" : "BREAK");

   // O "influencia os demais": este no NAO alcanca outro player -- ele so
   // marca o pedido. Quem transmite e o AlertDatalink, na fase 1 do frame
   // seguinte, com a mensagem chegando aos outros como evento nativo.
   //
   // So se avisa o que se esta VENDO: no arrasto da histerese a posicao do
   // contato ja e velha, e retransmiti-la manteria os outros convergindo
   // para um ponto que nao vale mais.
   if (policy.contactLive()) {
      decision.broadcastAlert = true;
      decision.alertContactName = snap.contactName;
      decision.alertNorthM = snap.contactNorthM;
      decision.alertEastM = snap.contactEastM;
      decision.alertAltitudeM = snap.contactAltitudeM;
      decision.alertRangeM = snap.contactRangeM;
   }

   return BT::NodeStatus::SUCCESS;
}

} // namespace bt_nodes
