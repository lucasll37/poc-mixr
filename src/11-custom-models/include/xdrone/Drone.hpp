#ifndef __xdrone_Drone_H__
#define __xdrone_Drone_H__

#include "mixr/models/player/Player.hpp"

namespace mixr {
namespace base { class Number; }

namespace xdrone {
class BtPilot;
class DroneDynamics;
class FuelSystem;
class ProximitySensor;

//------------------------------------------------------------------------------
// Class: Drone
//
// Description: Player proprio desta PoC. NAO herda de Aircraft nem de
//              AirVehicle: herda direto de models::Player, que e a classe
//              que o WorldModel realmente manipula (lista de players,
//              ciclo de 4 fases, gravacao REID_PLAYER_DATA, rede...).
//              Aircraft/AirVehicle/GroundVehicle sao, eles proprios,
//              subclasses finas de Player -- este arquivo faz exatamente o
//              mesmo que elas fazem, so que com os subsistemas desta poc.
//
// Factory name: Drone
//
// Slots:
//    emptyWeight  <Number>  ! Peso vazio, kg (default: 25)
//
// Herdados de Player (nao precisam ser redeclarados aqui): id, side, type,
// initXPos/initYPos/initAlt/initHeading/initVelocity, dataLogTime,
// signature, components, ...
//
// DESCOBERTA DE SUBSISTEMAS -- Player nao tem slot para cada subsistema:
// tudo entra por 'components' e e localizado POR TIPO em
// updateSystemPointers(). Sobrescrevemos esse metodo para achar tambem os
// nossos (FuelSystem/ProximitySensor/BtPilot), exatamente como o Player
// nativo acha DynamicsModel/Datalink/Gimbal/OnboardComputer/etc. Por isso a
// ordem dos componentes no .epp e irrelevante, e um Drone sem BtPilot
// simplesmente nao decide nada (nao e erro).
//
// Os ponteiros abaixo NAO sao ref()'d: quem e dono dos componentes e a
// lista de componentes do proprio player (mesma justificativa do ponteiro
// 'ownship' em models::System).
//------------------------------------------------------------------------------
class Drone : public models::Player
{
   DECLARE_SUBCLASS(Drone, models::Player)

public:
   Drone();

   unsigned int getMajorType() const override;   // AIR_VEHICLE
   double getGrossWeight() const override;       // lbs (contrato do framework)

   double getEmptyWeightKg() const   { return emptyWeightKg; }
   double getFuelKg() const;

   DroneDynamics* getDroneDynamics()        { return droneDynamics; }
   FuelSystem* getFuelSystem()              { return fuelSystem; }
   ProximitySensor* getProximitySensor()    { return proximitySensor; }
   BtPilot* getBtPilot()                    { return btPilot; }

protected:
   void updateSystemPointers() override;

private:
   double emptyWeightKg{25.0};

   DroneDynamics* droneDynamics{};
   FuelSystem* fuelSystem{};
   ProximitySensor* proximitySensor{};
   BtPilot* btPilot{};

   // slot table helpers
   bool setSlotEmptyWeight(const base::Number* const);
};

} // namespace xdrone
} // namespace mixr

#endif
