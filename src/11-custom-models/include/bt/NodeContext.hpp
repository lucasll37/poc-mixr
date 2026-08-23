#pragma once

namespace mixr {
namespace models { class DynamicsModel; }
namespace xdrone { class BtPilot; class Drone; class FuelSystem; class ProximitySensor; }
}

namespace bt_nodes {

//------------------------------------------------------------------------------
// NodeContext -- dependencias fixas de um no da arvore (os objetos MIXR
// daquela aeronave).
//
// CONVENCAO DO BehaviorTree.CPP: dependencias fixas entram pelo CONSTRUTOR
// do no, registrado com factory.registerBuilder<T>(ID, builder) -- e a
// forma que o autor da biblioteca documenta para "passar argumentos ao
// construtor" na v3 (a sobrecarga variadica de registerNodeType<T>(ID,
// args...) so existe em versoes posteriores; aqui a lib e a 3.5.6).
//
// O BLACKBOARD nao e usado para isso de proposito: ele existe para DADOS
// que fluem ENTRE nos durante o tick, e o proprio manual desaconselha
// usa-lo como saco de ponteiros globais. Configuracao que o operador
// mexe fica nos slots EDL (convencao MIXR) ou em ports do XML (convencao
// BT.CPP) -- ver o port 'margin' de FuelLowCondition.
//------------------------------------------------------------------------------
struct NodeContext
{
   mixr::xdrone::BtPilot* pilot{};
   mixr::xdrone::Drone* drone{};
   mixr::xdrone::FuelSystem* fuel{};
   mixr::xdrone::ProximitySensor* sensor{};
   mixr::models::DynamicsModel* dynamics{};

   bool complete() const
   {
      return pilot != nullptr && drone != nullptr && fuel != nullptr
          && sensor != nullptr && dynamics != nullptr;
   }
};

} // namespace bt_nodes
