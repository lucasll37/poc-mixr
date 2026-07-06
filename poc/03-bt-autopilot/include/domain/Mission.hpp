#pragma once

#include <cstddef>
#include <vector>

namespace domain {

// Um ponto da rota de patrulha: heading/altitude/velocidade alvo.
struct Waypoint
{
   double headingDeg{};
   double altitudeFt{};
   double speedKts{};
};

//------------------------------------------------------------------------------
// Class: Mission
//
// Regras de negocio da missao, sem qualquer dependencia do MIXR ou do
// BehaviorTree.CPP (pode ser testada isoladamente): patrulha ciclica entre
// waypoints, consumo de combustivel ao longo do tempo, e a decisao de
// quando entrar/sair do modo "retorno a base" (RTB) por combustivel baixo.
//
// Os nos da arvore de comportamento (bt/nodes/) sao adaptadores finos que
// leem esse estado e traduzem em comandos para o dynamics model do MIXR;
// toda a logica de "o que fazer" mora aqui.
//------------------------------------------------------------------------------
class Mission
{
public:
   Mission();

   // Avanca o relogio da missao (consome combustivel, acumula tempo no
   // waypoint atual ou no RTB, conforme o estado corrente).
   void tick(const double dt);

   bool isFuelLow() const;
   bool isInRtb() const;
   double getFuel() const;

   const Waypoint& currentWaypoint() const;
   const Waypoint& baseWaypoint() const;
   std::size_t currentWaypointIndex() const;

   // Se o tempo de permanencia no waypoint atual expirou, avanca para o
   // proximo (ciclico) e retorna true.
   bool maybeAdvanceWaypoint();

   // Marca o inicio do modo RTB (idempotente).
   void enterRtb();

   // Se o tempo de RTB expirou, reabastece e retorna true (RTB concluido).
   bool maybeCompleteRtb();

private:
   std::vector<Waypoint> waypoints_;
   std::size_t currentWaypointIndex_{0};

   double fuel_{100.0};
   static constexpr double kBurnRatePerSec{3.0};
   static constexpr double kLowFuelThreshold{30.0};

   double waypointElapsed_{0.0};
   static constexpr double kWaypointDwellSec{8.0};

   bool inRtb_{false};
   double rtbElapsed_{0.0};
   static constexpr double kRtbDwellSec{6.0};
};

} // namespace domain
