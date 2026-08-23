#ifndef __xdrone_FuelSystem_H__
#define __xdrone_FuelSystem_H__

#include "mixr/models/system/System.hpp"

#include <atomic>

namespace mixr {
namespace base { class Number; }

namespace xdrone {

//------------------------------------------------------------------------------
// Class: FuelSystem
//
// Description: Subsistema proprio (nao e o FuelTank do framework) que
//              consome combustivel em funcao da velocidade e reabastece
//              quando comandado. Serve de exemplo minimo de "como se
//              escreve um System" nesta PoC.
//
// Factory name: FuelSystem
//
// Slots:
//    capacity     <Number>   ! Capacidade, kg (default: 12)
//    initFuel     <Number>   ! Combustivel inicial, kg (default: capacity)
//    burnRate     <Number>   ! Consumo na velocidade de cruzeiro, kg/s (default: 0.05)
//    cruiseSpeed  <Number>   ! Velocidade de referencia do consumo, kts (default: 120)
//    reserve      <Number>   ! Fracao 0..1 que dispara o RTB (default: 0.30)
//    refuelRate   <Number>   ! Reabastecimento, kg/s (default: 1.5)
//
// FASE: o consumo roda em dynamics() -- FASE 0 do frame de tempo critico,
// a mesma em que a fisica anda. E a escolha certa porque consumo e uma
// consequencia direta do movimento daquele frame, nao uma decisao. Compare
// com o ProximitySensor (fase 2, "sensores recebem") e o BtPilot (fase 3,
// "logica"): as tres classes desta poc ocupam fases diferentes de proposito.
//
// THREADS: com numTcThreads > 1, esta funcao roda em uma das threads do
// pool nativo. O nivel de combustivel e lido pelo laco de background do
// main.cpp (outra thread), por isso e um std::atomic -- e nao um double
// solto.
//
// SIMPLIFICACAO ASSUMIDA: chegar a zero NAO corta o motor -- o drone
// continua voando com 0%. Modelar pane seca exigiria o FuelSystem
// interferir na dinamica (ou o Player entrar em CRASH), o que sai do
// escopo deste exemplo. Na pratica isso nunca acontece com a reserva
// calibrada no scenario.epp, mas e uma limitacao real desta classe.
//------------------------------------------------------------------------------
class FuelSystem : public models::System
{
   DECLARE_SUBCLASS(FuelSystem, models::System)

public:
   FuelSystem();

   void reset() override;

   double getFuelKg() const              { return fuelKg.load(std::memory_order_relaxed); }
   double getCapacityKg() const          { return capacityKg; }
   double getFraction() const;
   double getReserveFraction() const     { return reserveFraction; }

   bool isBelowReserve() const           { return getFraction() < reserveFraction; }
   bool isFull() const                   { return getFraction() >= 0.999; }

   bool isRefueling() const              { return refueling.load(std::memory_order_relaxed); }
   void setRefueling(const bool b)       { refueling.store(b, std::memory_order_relaxed); }

protected:
   // FASE 0 -- consumo/reabastecimento
   void dynamics(const double dt) override;

private:
   double capacityKg{12.0};
   double initFuelKg{-1.0};        // <0 => "usa a capacidade"
   double burnRateKgPerSec{0.05};
   double cruiseSpeedKts{120.0};
   double reserveFraction{0.30};
   double refuelRateKgPerSec{1.5};

   std::atomic<double> fuelKg{};
   std::atomic<bool> refueling{false};

   // slot table helpers
   bool setSlotCapacity(const base::Number* const);
   bool setSlotInitFuel(const base::Number* const);
   bool setSlotBurnRate(const base::Number* const);
   bool setSlotCruiseSpeed(const base::Number* const);
   bool setSlotReserve(const base::Number* const);
   bool setSlotRefuelRate(const base::Number* const);
};

} // namespace xdrone
} // namespace mixr

#endif
