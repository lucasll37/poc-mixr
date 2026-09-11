#include "bt/bt_factory.hpp"

#include "bt/nodes/AlertReceivedCondition.hpp"
#include "bt/nodes/ContactDetectedCondition.hpp"
#include "bt/nodes/FuelLowCondition.hpp"
#include "bt/nodes/LaunchEnvelopeCondition.hpp"
#include "bt/nodes/LaunchMissileAction.hpp"
#include "bt/nodes/NavigateAction.hpp"
#include "bt/nodes/PatrolAction.hpp"
#include "bt/nodes/ReportAndEvadeAction.hpp"
#include "bt/nodes/ReturnToBaseAction.hpp"
#include "bt/nodes/SlowRollAction.hpp"
#include "bt/nodes/SupportAlertAction.hpp"

namespace bt_nodes {

namespace {

// registerBuilder<T>(ID, builder) e o ponto de extensao do BehaviorTree.CPP
// v3 para construtores com argumentos extras (a sobrecarga variadica de
// registerNodeType so existe em versoes posteriores).
template <typename NodeType>
void registerWithContext(BT::BehaviorTreeFactory& factory, const std::string& id,
                         const NodeContext& context)
{
   BT::NodeBuilder builder{
      [context](const std::string& name, const BT::NodeConfiguration& config) {
         return std::make_unique<NodeType>(name, config, context);
      }};
   factory.registerBuilder<NodeType>(id, builder);
}

}

// Registrar um no aqui e' so metade do trabalho: o Groot (deps/groot/,
// CLAUDE.md "Groot -- editor e monitor ao vivo") NAO enxerga estas classes --
// ele e' um app a parte, nunca viu este .so. Os 5 configs/flight_tree*.xml
// de producao carregam um <TreeNodesModel> colado a mao, com o MESMO ID
// desta chamada, so' pra ele reconhecer os nos. Registrou um no novo aqui ou
// em bt_factory_sdk.cpp? Atualize o bloco nos 5 arquivos tambem, ou o Groot
// recusa a arvore com "This model has not been registered: <ID>".
void registerNodes(BT::BehaviorTreeFactory& factory, const NodeContext& context)
{
   registerWithContext<FuelLowCondition>(factory, "FuelLow", context);
   registerWithContext<ReturnToBaseAction>(factory, "ReturnToBase", context);
   registerWithContext<LaunchEnvelopeCondition>(factory, "LaunchEnvelope", context);
   registerWithContext<LaunchMissileAction>(factory, "LaunchMissile", context);
   registerWithContext<ContactDetectedCondition>(factory, "ContactDetected", context);
   registerWithContext<ReportAndEvadeAction>(factory, "ReportAndEvade", context);
   registerWithContext<AlertReceivedCondition>(factory, "AlertReceived", context);
   registerWithContext<SupportAlertAction>(factory, "SupportAlert", context);
   registerWithContext<PatrolAction>(factory, "Patrol", context);
   registerWithContext<NavigateAction>(factory, "Navigate", context);
   registerWithContext<SlowRollAction>(factory, "SlowRoll", context);
}

} // namespace bt_nodes
