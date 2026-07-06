//
// poc/02-behavior-tree
//
// Subprojeto minimo e funcional usando BehaviorTree.CPP (v3, pacote Conan
// behaviortree.cpp.asa/3.5.6). Nao depende do MIXR -- e uma prova de
// conceito isolada da biblioteca de arvores de comportamento, pensada como
// base para, num proximo ciclo, decidir o comportamento de um player MIXR
// (ex.: substituir o autopilot manual do poc/01-flying-aircraft).
//
// Cenario: um "sentinela" que patrulha enquanto tem bateria e recarrega
// quando ela fica baixa. A arvore (configs/tree.xml) e:
//
//   Fallback
//   +- Sequence
//   |  +- BatteryLow   (Condition)
//   |  +- Recharge     (Action)
//   +- Patrol          (Action)
//
// O estado "battery" fica no blackboard raiz, compartilhado entre os nos.
//

#include "behaviortree_cpp_v3/bt_factory.h"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

namespace {

const char* const kBatteryKey = "battery";
const int kDrainPerTick{15};
const int kLowThreshold{30};
const int kFullBattery{100};

BT::NodeStatus checkBatteryLow(BT::TreeNode& node)
{
   int battery{};
   node.config().blackboard->get<int>(kBatteryKey, battery);
   return battery < kLowThreshold ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
}

BT::NodeStatus patrol(BT::TreeNode& node)
{
   int battery{};
   node.config().blackboard->get<int>(kBatteryKey, battery);
   battery = std::max(0, battery - kDrainPerTick);
   node.config().blackboard->set<int>(kBatteryKey, battery);

   std::cout << "  [Patrol]   patrulhando...   battery=" << battery << std::endl;
   return BT::NodeStatus::SUCCESS;
}

BT::NodeStatus recharge(BT::TreeNode& node)
{
   node.config().blackboard->set<int>(kBatteryKey, kFullBattery);

   std::cout << "  [Recharge] recarregando...  battery=" << kFullBattery << std::endl;
   return BT::NodeStatus::SUCCESS;
}

} // namespace

int main(int argc, char* argv[])
{
   std::string treeFilename = "./poc/02-behavior-tree/configs/tree.xml";

   for (int i = 1; i < argc; i++) {
      if (std::string(argv[i]) == "-f") {
         treeFilename = argv[++i];
      }
   }

   BT::BehaviorTreeFactory factory;
   factory.registerSimpleCondition("BatteryLow", checkBatteryLow);
   factory.registerSimpleAction("Patrol", patrol);
   factory.registerSimpleAction("Recharge", recharge);

   auto blackboard = BT::Blackboard::create();
   blackboard->set<int>(kBatteryKey, kFullBattery);

   BT::Tree tree = factory.createTreeFromFile(treeFilename, blackboard);

   std::cout << "=== poc/02-behavior-tree ===" << std::endl;

   const int numTicks{20};
   for (int i = 0; i < numTicks; i++) {
      std::cout << "tick " << (i + 1) << ":" << std::endl;
      tree.tickRoot();
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
   }

   std::cout << "=== fim ===" << std::endl;
   return 0;
}
