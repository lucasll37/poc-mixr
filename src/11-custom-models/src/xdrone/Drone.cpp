#include "xdrone/Drone.hpp"

#include "xdrone/BtPilot.hpp"
#include "xdrone/DroneDynamics.hpp"
#include "xdrone/FuelSystem.hpp"
#include "xdrone/ProximitySensor.hpp"

#include "mixr/base/Pair.hpp"
#include "mixr/base/numeric/Number.hpp"
#include "mixr/base/units/mass_utils.hpp"

namespace mixr {
namespace xdrone {

IMPLEMENT_SUBCLASS(Drone, "Drone")

BEGIN_SLOTTABLE(Drone)
   "emptyWeight",   // 1: kg
END_SLOTTABLE(Drone)

BEGIN_SLOT_MAP(Drone)
   ON_SLOT(1, setSlotEmptyWeight, base::Number)
END_SLOT_MAP()

EMPTY_DELETEDATA(Drone)

Drone::Drone()
{
   STANDARD_CONSTRUCTOR()

   // Nada a inicializar alem dos defaults dos membros: id/side/type/
   // posicao inicial vem dos slots herdados de Player, preenchidos pelo
   // edl_parser a partir do scenario.epp.
}

void Drone::copyData(const Drone& org, const bool)
{
   BaseClass::copyData(org);

   emptyWeightKg = org.emptyWeightKg;

   // Ponteiros de subsistema NAO sao copiados: a copia tem a propria lista
   // de componentes e os resolve no proximo updateSystemPointers().
   droneDynamics = nullptr;
   fuelSystem = nullptr;
   proximitySensor = nullptr;
   btPilot = nullptr;
}

unsigned int Drone::getMajorType() const
{
   // O major type e o que o resto do framework usa para filtrar players
   // (sensores com playerOfInterestTypes, ground clamping, mapeamento de
   // tipo ACMI no TacviewOutput...). Um drone e um veiculo aereo.
   return AIR_VEHICLE;
}

double Drone::getFuelKg() const
{
   return (fuelSystem != nullptr) ? fuelSystem->getFuelKg() : 0.0;
}

double Drone::getGrossWeight() const
{
   // Contrato do framework: Player::getGrossWeight() e em LIBRAS. Os
   // slots desta poc sao em kg (SI), entao a conversao acontece aqui, na
   // fronteira -- em vez de deixar duas unidades circulando pelo codigo.
   return (emptyWeightKg + getFuelKg()) * base::mass::KG2PM;
}

//------------------------------------------------------------------------------
// updateSystemPointers() -- localizacao dos subsistemas POR TIPO.
//
// Chamado pelo Player sempre que a lista de componentes muda (flag
// loadSysPtrs) e no reset. BaseClass resolve os subsistemas nativos; aqui
// resolvemos os nossos.
//------------------------------------------------------------------------------
void Drone::updateSystemPointers()
{
   BaseClass::updateSystemPointers();

   droneDynamics = nullptr;
   fuelSystem = nullptr;
   proximitySensor = nullptr;
   btPilot = nullptr;

   {
      base::Pair* const p{findByType(typeid(DroneDynamics))};
      if (p != nullptr) droneDynamics = dynamic_cast<DroneDynamics*>(p->object());
   }
   {
      base::Pair* const p{findByType(typeid(FuelSystem))};
      if (p != nullptr) fuelSystem = dynamic_cast<FuelSystem*>(p->object());
   }
   {
      base::Pair* const p{findByType(typeid(ProximitySensor))};
      if (p != nullptr) proximitySensor = dynamic_cast<ProximitySensor*>(p->object());
   }
   {
      base::Pair* const p{findByType(typeid(BtPilot))};
      if (p != nullptr) btPilot = dynamic_cast<BtPilot*>(p->object());
   }
}

//------------------------------------------------------------------------------
// slots
//------------------------------------------------------------------------------
bool Drone::setSlotEmptyWeight(const base::Number* const msg)
{
   if (msg == nullptr) return false;
   emptyWeightKg = msg->getDouble();
   return (emptyWeightKg > 0.0);
}

} // namespace xdrone
} // namespace mixr
