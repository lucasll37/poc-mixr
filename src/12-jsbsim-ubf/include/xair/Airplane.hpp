#ifndef __xair_Airplane_H__
#define __xair_Airplane_H__

#include "mixr/models/player/Player.hpp"

#include <mutex>
#include <string>

namespace mixr {
namespace xair {
class AlertRadio;
class FlightDirector;
class JsbsimFlightModel;
class ProximitySensor;

//------------------------------------------------------------------------------
// Class: Airplane
//
// Description: Player proprio desta PoC -- um caca. Herda direto de
//              models::Player (nao de Aircraft/AirVehicle), como o Drone da
//              poc/11, e localiza os subsistemas POR TIPO.
//
// Factory name: Airplane
//
// Slots: (nenhum proprio -- id/side/type/init*/dataLogTime vem de Player)
//
// DIFERENCA PARA O Drone DA poc/11: aqui NAO ha 'dynamicsModel' nenhum.
// getDynamicsModel() devolve nullptr o tempo todo, e quem move a aeronave e
// o JsbsimFlightModel (um System, fase 0) escrevendo a posicao com
// slaved=true. Ver o cabecalho daquela classe.
//
// 'behaviorLabel' e so observabilidade: a acao da UBF carimba aqui o nome
// do comportamento que venceu no frame, e o main.cpp imprime. Fica no
// player (e nao no agente) porque e o player que o status lista.
//------------------------------------------------------------------------------
class Airplane : public models::Player
{
   DECLARE_SUBCLASS(Airplane, models::Player)

public:
   Airplane();

   unsigned int getMajorType() const override;   // AIR_VEHICLE

   JsbsimFlightModel* getFlightModel()     { return flightModel; }
   FlightDirector* getFlightDirector()     { return flightDirector; }
   ProximitySensor* getProximitySensor()   { return proximitySensor; }
   AlertRadio* getAlertRadio()             { return alertRadio; }

   // Versoes const -- o UBF entrega o ator como 'const Component*' para o
   // AbstractState (percepcao NAO deve mexer no ator).
   const JsbsimFlightModel* getFlightModel() const     { return flightModel; }
   const FlightDirector* getFlightDirector() const     { return flightDirector; }
   const ProximitySensor* getProximitySensor() const   { return proximitySensor; }
   const AlertRadio* getAlertRadio() const             { return alertRadio; }

   void setBehaviorLabel(const std::string&);
   std::string getBehaviorLabel() const;

protected:
   void updateSystemPointers() override;

private:
   // Nao ref()'d: quem e dono e a lista de componentes do proprio player.
   JsbsimFlightModel* flightModel{};
   FlightDirector* flightDirector{};
   ProximitySensor* proximitySensor{};
   AlertRadio* alertRadio{};

   mutable std::mutex labelMutex;
   std::string behaviorLabel{"INIT"};
};

} // namespace xair
} // namespace mixr

#endif
