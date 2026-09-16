#include "ubf/ObservationBridge.hpp"

#include "xrlbridge/ObservationFields.hpp"

namespace mixr {
namespace models {
namespace xA_4 {

xrlbridge::Observation toObservation(const domain::WorldView& snap)
{
   xrlbridge::Observation obs;
#define XRLBRIDGE_F(nome) obs.nome = snap.nome;
#define XRLBRIDGE_B(nome) obs.nome = snap.nome;
   XRLBRIDGE_OBSERVATION_FIELDS
#undef XRLBRIDGE_F
#undef XRLBRIDGE_B
   obs.contactName = snap.contactName;
   obs.alertSender = snap.alertSender;
   obs.alertContactName = snap.alertContactName;
   obs.ownerName = snap.ownerName;
   return obs;
}

} // namespace xA_4
} // namespace models
} // namespace mixr
