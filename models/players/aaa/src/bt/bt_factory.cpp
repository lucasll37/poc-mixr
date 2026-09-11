#include "bt/bt_factory.hpp"

#include "bt/nodes/FireMissileAction.hpp"
#include "bt/nodes/TargetInDomeCondition.hpp"
#include "bt/nodes/WatchAction.hpp"

namespace mixr {
namespace models {
namespace xaaa {
namespace bt {

namespace {

// registerBuilder<T>(ID, builder) e' o ponto de extensao do BehaviorTree.CPP
// v3 para construtores com argumentos extras (a sobrecarga variadica de
// registerNodeType so' existe em versoes posteriores -- este repositorio
// esta preso na 3.5.6, ver contexts/BTCPP-CONTEXT.md).
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

} // namespace

// O ID (primeiro argumento) e' o nome da TAG no XML, nao o nome da classe
// C++ -- e' o que o Groot mostra na paleta e o que configs/*.xml escreve.
//
// Registrou um no novo aqui? Rode 'make update-bt' para o <TreeNodesModel>
// de toda arvore de configs/ acompanhar; sem isso o Groot recusa a arvore
// com "This model has not been registered: <ID>". O teste 'tree-model-sync'
// (suite 'tree') pega o esquecimento sozinho.
void registerNodes(BT::BehaviorTreeFactory& factory, const NodeContext& context)
{
   registerWithContext<TargetInDomeCondition>(factory, "TargetInDome", context);
   registerWithContext<FireMissileAction>(factory, "FireMissile", context);
   registerWithContext<WatchAction>(factory, "Watch", context);
}

} // namespace bt
} // namespace xaaa
} // namespace models
} // namespace mixr
