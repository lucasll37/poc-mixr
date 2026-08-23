#include "bt/nodes/FuelLowCondition.hpp"

#include "xdrone/FuelSystem.hpp"

namespace bt_nodes {

FuelLowCondition::FuelLowCondition(const std::string& name, const BT::NodeConfiguration& config,
                                   const NodeContext& context)
   : BT::ConditionNode(name, config), context_(context)
{
}

BT::PortsList FuelLowCondition::providedPorts()
{
   return { BT::InputPort<double>("margin", 0.0,
                                  "margem somada a reserva do tanque (fracao 0..1)") };
}

BT::NodeStatus FuelLowCondition::tick()
{
   const auto fuel = context_.fuel;
   if (fuel == nullptr) return BT::NodeStatus::FAILURE;

   // getInput() devolve Optional: se o atributo nao estiver no XML, cai no
   // default declarado em providedPorts().
   double margin{};
   const BT::Optional<double> input{getInput<double>("margin")};
   if (input) margin = input.value();

   const bool low{fuel->getFraction() < (fuel->getReserveFraction() + margin)};
   return (low || fuel->isRefueling()) ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
}

} // namespace bt_nodes
