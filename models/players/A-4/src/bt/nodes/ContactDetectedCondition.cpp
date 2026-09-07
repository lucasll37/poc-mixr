#include "bt/nodes/ContactDetectedCondition.hpp"

#include "bt/DecisionContext.hpp"

namespace bt_nodes {

ContactDetectedCondition::ContactDetectedCondition(const std::string& name,
                                                   const BT::NodeConfiguration& config,
                                                   const NodeContext& context)
   : BT::ConditionNode(name, config), context_(context)
{
}

//------------------------------------------------------------------------------
// A condicao NAO e "estou vendo o intruso agora", e sim "a manobra de evasao
// esta valendo" -- que continua true por alguns segundos depois de a pista
// sumir (domain::ThreatPolicy::engaged()).
//
// E essa diferenca que impede a alternancia com o ramo de apoio: a propria
// quebra tira o intruso do setor do radar (+-30 graus, contra uma quebra de
// 110), entao "vendo agora" pisca -- e o ramo de baixo assumia, trazia a
// aeronave de volta e ela reaquisitava. Resultado observado no Tacview:
// aeronaves oscilando +-25 graus de banco, com periodo de ~24 s.
//------------------------------------------------------------------------------
BT::NodeStatus ContactDetectedCondition::tick()
{
   if (context_.behavior == nullptr) return BT::NodeStatus::FAILURE;

   return context_.behavior->threatPolicy().engaged() ? BT::NodeStatus::SUCCESS
                                                      : BT::NodeStatus::FAILURE;
}

} // namespace bt_nodes
