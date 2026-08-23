#include "bt/nodes/FuelLowCondition.hpp"

#include "ubf/BtBehavior.hpp"

namespace bt_nodes {

FuelLowCondition::FuelLowCondition(const std::string& name, const BT::NodeConfiguration& config,
                                   const NodeContext& context)
   : BT::ConditionNode(name, config), context_(context)
{
}

BT::PortsList FuelLowCondition::providedPorts()
{
   return { BT::InputPort<double>("margin", 0.0,
                                  "margem somada a reserva de combustivel (fracao 0..1)") };
}

BT::NodeStatus FuelLowCondition::tick()
{
   if (context_.behavior == nullptr) return BT::NodeStatus::FAILURE;

   double margin{};
   const BT::Optional<double> input{getInput<double>("margin")};
   if (input) margin = input.value();

   const auto& snap = context_.behavior->snapshot();
   const bool low{snap.fuelFraction < (context_.behavior->getFuelReserve() + margin)};
   return low ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
}

} // namespace bt_nodes
