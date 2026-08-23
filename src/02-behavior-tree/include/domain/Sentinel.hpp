#pragma once

namespace domain {

//------------------------------------------------------------------------------
// Class: Sentinel
// Description: Regras de negocio do sentinela -- bateria, patrulha e
//              recarga. NAO inclui nada de BehaviorTree.CPP nem do MIXR:
//              e o "o que fazer", testavel isoladamente.
//
// Mesma separacao de camadas de poc/03-bt-autopilot: domain/ decide,
// bt/nodes/ apenas adapta essas decisoes para a arvore.
//------------------------------------------------------------------------------
class Sentinel
{
public:
   Sentinel() = default;

   int battery() const            { return battery_; }
   bool batteryLow() const        { return battery_ < lowThreshold_; }
   int patrolLaps() const         { return patrolLaps_; }

   // Uma volta de patrulha: gasta bateria (nunca abaixo de zero).
   void patrol();

   // Recarga completa.
   void recharge();

private:
   static const int drainPerLap_{15};
   static const int lowThreshold_{30};
   static const int fullBattery_{100};

   int battery_{fullBattery_};
   int patrolLaps_{};
};

}
