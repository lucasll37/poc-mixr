#include "xdrone/FuelSystem.hpp"

#include "mixr/models/player/Player.hpp"

#include "mixr/base/numeric/Number.hpp"
#include "mixr/base/units/distance_utils.hpp"

namespace mixr {
namespace xdrone {

IMPLEMENT_SUBCLASS(FuelSystem, "FuelSystem")

BEGIN_SLOTTABLE(FuelSystem)
   "capacity",      // 1: kg
   "initFuel",      // 2: kg
   "burnRate",      // 3: kg/s na velocidade de cruzeiro
   "cruiseSpeed",   // 4: kts
   "reserve",       // 5: fracao 0..1
   "refuelRate",    // 6: kg/s
END_SLOTTABLE(FuelSystem)

BEGIN_SLOT_MAP(FuelSystem)
   ON_SLOT(1, setSlotCapacity,    base::Number)
   ON_SLOT(2, setSlotInitFuel,    base::Number)
   ON_SLOT(3, setSlotBurnRate,    base::Number)
   ON_SLOT(4, setSlotCruiseSpeed, base::Number)
   ON_SLOT(5, setSlotReserve,     base::Number)
   ON_SLOT(6, setSlotRefuelRate,  base::Number)
END_SLOT_MAP()

EMPTY_DELETEDATA(FuelSystem)

namespace {
const double KTS2MPS{base::distance::NM2M / 3600.0};
}

FuelSystem::FuelSystem()
{
   STANDARD_CONSTRUCTOR()
}

void FuelSystem::copyData(const FuelSystem& org, const bool)
{
   BaseClass::copyData(org);

   capacityKg = org.capacityKg;
   initFuelKg = org.initFuelKg;
   burnRateKgPerSec = org.burnRateKgPerSec;
   cruiseSpeedKts = org.cruiseSpeedKts;
   reserveFraction = org.reserveFraction;
   refuelRateKgPerSec = org.refuelRateKgPerSec;

   fuelKg.store(org.fuelKg.load());
   refueling.store(org.refueling.load());
}

void FuelSystem::reset()
{
   BaseClass::reset();

   const double initial{(initFuelKg >= 0.0) ? initFuelKg : capacityKg};
   fuelKg.store((initial > capacityKg) ? capacityKg : initial);
   refueling.store(false);
}

double FuelSystem::getFraction() const
{
   if (capacityKg <= 0.0) return 0.0;
   return getFuelKg() / capacityKg;
}

//------------------------------------------------------------------------------
// FASE 0 -- consumo proporcional a velocidade, ou reabastecimento.
//------------------------------------------------------------------------------
void FuelSystem::dynamics(const double dt)
{
   if (dt <= 0.0) return;

   double fuel{fuelKg.load(std::memory_order_relaxed)};

   if (refueling.load(std::memory_order_relaxed)) {
      fuel += refuelRateKgPerSec * dt;
      if (fuel > capacityKg) fuel = capacityKg;
      fuelKg.store(fuel, std::memory_order_relaxed);
      return;
   }

   const models::Player* const player{getOwnship()};
   double speedKts{cruiseSpeedKts};
   if (player != nullptr) speedKts = player->getTotalVelocity() / KTS2MPS;

   // Consumo proporcional a velocidade, limitado para nao virar zero nem
   // explodir num transiente do dynamics model.
   double factor{(cruiseSpeedKts > 0.0) ? (speedKts / cruiseSpeedKts) : 1.0};
   if (factor < 0.3) factor = 0.3;
   if (factor > 2.0) factor = 2.0;

   fuel -= burnRateKgPerSec * factor * dt;
   if (fuel < 0.0) fuel = 0.0;
   fuelKg.store(fuel, std::memory_order_relaxed);
}

//------------------------------------------------------------------------------
// slots
//------------------------------------------------------------------------------
bool FuelSystem::setSlotCapacity(const base::Number* const msg)
{
   if (msg == nullptr) return false;
   capacityKg = msg->getDouble();
   return (capacityKg > 0.0);
}

bool FuelSystem::setSlotInitFuel(const base::Number* const msg)
{
   if (msg == nullptr) return false;
   initFuelKg = msg->getDouble();
   return (initFuelKg >= 0.0);
}

bool FuelSystem::setSlotBurnRate(const base::Number* const msg)
{
   if (msg == nullptr) return false;
   burnRateKgPerSec = msg->getDouble();
   return (burnRateKgPerSec >= 0.0);
}

bool FuelSystem::setSlotCruiseSpeed(const base::Number* const msg)
{
   if (msg == nullptr) return false;
   cruiseSpeedKts = msg->getDouble();
   return (cruiseSpeedKts > 0.0);
}

bool FuelSystem::setSlotReserve(const base::Number* const msg)
{
   if (msg == nullptr) return false;
   reserveFraction = msg->getDouble();
   return (reserveFraction >= 0.0 && reserveFraction <= 1.0);
}

bool FuelSystem::setSlotRefuelRate(const base::Number* const msg)
{
   if (msg == nullptr) return false;
   refuelRateKgPerSec = msg->getDouble();
   return (refuelRateKgPerSec > 0.0);
}

} // namespace xdrone
} // namespace mixr
