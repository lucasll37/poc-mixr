//
// poc/02-behavior-tree
//
// Subprojeto minimo e funcional usando BehaviorTree.CPP (v3, pacote Conan
// behaviortree.cpp.asa/3.5.6). Nao depende do MIXR -- e uma prova de
// conceito isolada da biblioteca de arvores de comportamento.
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
// Estrutura em camadas (mesma de poc/03-bt-autopilot):
//   domain/Sentinel  -- regras puras (bateria, patrulha, recarga)
//   bt/nodes/*       -- adaptadores finos que leem/comandam o Sentinel
//   bt/bt_factory    -- registro dos nos + injecao via blackboard
//   main.cpp         -- so orquestra
//
// O estado NAO mora mais no blackboard como um int solto: o blackboard
// carrega um ponteiro para o objeto de dominio, que e quem guarda o estado.
//

#include "bt/bt_factory.hpp"
#include "domain/Sentinel.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

int main(int argc, char* argv[])
{
   std::string treeFilename = "./poc/02-behavior-tree/configs/tree.xml";

   for (int i = 1; i < argc; i++) {
      if (std::string(argv[i]) == "-f") {
         treeFilename = argv[++i];
      }
   }

   domain::Sentinel sentinel;

   BT::BehaviorTreeFactory factory;
   bt::registerNodes(factory);

   BT::Tree tree = factory.createTreeFromFile(treeFilename, bt::makeBlackboard(&sentinel));

   std::cout << "=== poc/02-behavior-tree ===" << std::endl;

   const int numTicks{20};
   for (int i = 0; i < numTicks; i++) {
      std::cout << "tick " << (i + 1) << ":" << std::endl;
      tree.tickRoot();
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
   }

   std::cout << "=== fim ===" << std::endl;
   std::cout << "voltas de patrulha: " << sentinel.patrolLaps() << std::endl;
   return 0;
}
