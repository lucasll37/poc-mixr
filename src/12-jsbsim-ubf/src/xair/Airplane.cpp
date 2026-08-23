#include "xair/Airplane.hpp"

#include "xair/AlertRadio.hpp"
#include "xair/FlightDirector.hpp"
#include "xair/JsbsimFlightModel.hpp"
#include "xair/ProximitySensor.hpp"

#include "mixr/base/Pair.hpp"

namespace mixr {
namespace xair {

IMPLEMENT_SUBCLASS(Airplane, "Airplane")
EMPTY_SLOTTABLE(Airplane)
EMPTY_DELETEDATA(Airplane)

Airplane::Airplane()
{
   STANDARD_CONSTRUCTOR()
}

void Airplane::copyData(const Airplane& org, const bool)
{
   BaseClass::copyData(org);

   flightModel = nullptr;
   flightDirector = nullptr;
   proximitySensor = nullptr;
   alertRadio = nullptr;
   behaviorLabel = "INIT";
}

unsigned int Airplane::getMajorType() const
{
   return AIR_VEHICLE;
}

void Airplane::setBehaviorLabel(const std::string& label)
{
   std::lock_guard<std::mutex> lock(labelMutex);
   behaviorLabel = label;
}

std::string Airplane::getBehaviorLabel() const
{
   std::lock_guard<std::mutex> lock(labelMutex);
   return behaviorLabel;
}

void Airplane::updateSystemPointers()
{
   BaseClass::updateSystemPointers();

   flightModel = nullptr;
   flightDirector = nullptr;
   proximitySensor = nullptr;
   alertRadio = nullptr;

   {
      base::Pair* const p{findByType(typeid(JsbsimFlightModel))};
      if (p != nullptr) flightModel = dynamic_cast<JsbsimFlightModel*>(p->object());
   }
   {
      base::Pair* const p{findByType(typeid(FlightDirector))};
      if (p != nullptr) flightDirector = dynamic_cast<FlightDirector*>(p->object());
   }
   {
      base::Pair* const p{findByType(typeid(ProximitySensor))};
      if (p != nullptr) proximitySensor = dynamic_cast<ProximitySensor*>(p->object());
   }
   {
      base::Pair* const p{findByType(typeid(AlertRadio))};
      if (p != nullptr) alertRadio = dynamic_cast<AlertRadio*>(p->object());
   }
}

} // namespace xair
} // namespace mixr
