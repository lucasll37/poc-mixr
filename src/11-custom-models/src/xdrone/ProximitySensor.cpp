#include "xdrone/ProximitySensor.hpp"

#include "domain/geometry.hpp"

#include "mixr/models/WorldModel.hpp"
#include "mixr/models/player/Player.hpp"

#include "mixr/base/List.hpp"
#include "mixr/base/Pair.hpp"
#include "mixr/base/PairStream.hpp"
#include "mixr/base/Identifier.hpp"
#include "mixr/base/numeric/Number.hpp"
#include "mixr/base/units/Angles.hpp"
#include "mixr/base/units/Distances.hpp"
#include "mixr/base/units/Times.hpp"

#include <cmath>

namespace mixr {
namespace xdrone {

IMPLEMENT_SUBCLASS(ProximitySensor, "ProximitySensor")

BEGIN_SLOTTABLE(ProximitySensor)
   "maxRange",       // 1: alcance maximo
   "fieldOfView",    // 2: meio-angulo do setor a frente
   "scanInterval",   // 3: intervalo entre varreduras
   "holdTime",       // 4: memoria da pista apos perder o contato
   "hostileOnly",    // 5: !=0 => so outro 'side'
END_SLOTTABLE(ProximitySensor)

BEGIN_SLOT_MAP(ProximitySensor)
   ON_SLOT(1, setSlotMaxRange,     base::Distance)
   ON_SLOT(2, setSlotFieldOfView,  base::Angle)
   ON_SLOT(3, setSlotScanInterval, base::Time)
   ON_SLOT(4, setSlotHoldTime,     base::Time)
   ON_SLOT(5, setSlotHostileOnly,  base::Number)
END_SLOT_MAP()

EMPTY_DELETEDATA(ProximitySensor)

ProximitySensor::ProximitySensor()
{
   STANDARD_CONSTRUCTOR()
}

void ProximitySensor::copyData(const ProximitySensor& org, const bool)
{
   BaseClass::copyData(org);

   maxRangeM = org.maxRangeM;
   fovDeg = org.fovDeg;
   scanIntervalSec = org.scanIntervalSec;
   holdTimeSec = org.holdTimeSec;
   hostileOnly = org.hostileOnly;

   scanTimer = 0.0;
   holdTimer = 0.0;
   scanCount = 0;
   contact = Contact{};
}

void ProximitySensor::reset()
{
   BaseClass::reset();

   std::lock_guard<std::mutex> lock(contactMutex);
   contact = Contact{};
   scanTimer = 0.0;
   holdTimer = 0.0;
   scanCount = 0;
}

ProximitySensor::Contact ProximitySensor::getContact() const
{
   std::lock_guard<std::mutex> lock(contactMutex);
   return contact;
}

bool ProximitySensor::hasContact() const
{
   std::lock_guard<std::mutex> lock(contactMutex);
   return contact.valid;
}

long ProximitySensor::getScanCount() const
{
   std::lock_guard<std::mutex> lock(contactMutex);
   return scanCount;
}

//------------------------------------------------------------------------------
// FASE 2 -- varredura geometrica da lista de players.
//------------------------------------------------------------------------------
void ProximitySensor::receive(const double dt)
{
   if (dt <= 0.0) return;

   // Envelhecimento da pista roda TODO frame (nao so na varredura): e o
   // que da a histerese entre EVADE e PATROL descrita no cabecalho.
   {
      std::lock_guard<std::mutex> lock(contactMutex);
      if (contact.valid) {
         holdTimer -= dt;
         if (holdTimer <= 0.0) contact = Contact{};
      }
   }

   scanTimer -= dt;
   if (scanTimer > 0.0) return;
   scanTimer = scanIntervalSec;

   const models::Player* const own{getOwnship()};
   models::WorldModel* const world{getWorldModel()};
   if (own == nullptr || world == nullptr) return;

   const base::Vec3d ownPos{own->getPosition()};   // NED da area de jogo (m)
   const double ownN{ownPos[models::Player::INORTH]};
   const double ownE{ownPos[models::Player::IEAST]};
   const double ownAlt{own->getAltitudeM()};
   const double ownHdg{own->getHeadingD()};

   Contact best;

   base::PairStream* const players{world->getPlayers()};
   if (players != nullptr) {

      const base::List::Item* item{players->getFirstItem()};
      while (item != nullptr) {
         const auto pair = static_cast<const base::Pair*>(item->getValue());
         const auto other = dynamic_cast<const models::Player*>(pair->object());
         item = item->getNext();

         if (other == nullptr || other == own) continue;
         if (!other->isActive()) continue;
         if (hostileOnly && other->getSide() == own->getSide()) continue;

         const base::Vec3d otherPos{other->getPosition()};
         const domain::RelativeGeometry g{domain::relativeTo(
            ownN, ownE, ownAlt, ownHdg,
            otherPos[models::Player::INORTH], otherPos[models::Player::IEAST],
            other->getAltitudeM())};

         if (g.rangeM > maxRangeM) continue;
         if (std::fabs(g.relBearingDeg) > fovDeg) continue;
         if (best.valid && g.rangeM >= best.rangeM) continue;

         best.valid = true;
         best.rangeM = g.rangeM;
         best.relBearingDeg = g.relBearingDeg;
         best.deltaAltM = g.deltaAltM;
         best.name = (other->getName() != nullptr) ? other->getName()->getString() : "?";
      }

      players->unref();
   }

   std::lock_guard<std::mutex> lock(contactMutex);
   scanCount += 1;
   if (best.valid) {
      contact = best;
      holdTimer = holdTimeSec;
   }
   // Se a varredura nao achou nada, NAO apagamos o contato aqui: ele
   // expira sozinho quando holdTimer zerar (bloco no topo da funcao).
}

//------------------------------------------------------------------------------
// slots
//------------------------------------------------------------------------------
bool ProximitySensor::setSlotMaxRange(const base::Distance* const msg)
{
   if (msg == nullptr) return false;
   maxRangeM = base::Meters::convertStatic(*msg);
   return (maxRangeM > 0.0);
}

bool ProximitySensor::setSlotFieldOfView(const base::Angle* const msg)
{
   if (msg == nullptr) return false;
   fovDeg = base::Degrees::convertStatic(*msg);
   return (fovDeg > 0.0 && fovDeg <= 180.0);
}

bool ProximitySensor::setSlotScanInterval(const base::Time* const msg)
{
   if (msg == nullptr) return false;
   scanIntervalSec = base::Seconds::convertStatic(*msg);
   return (scanIntervalSec >= 0.0);
}

bool ProximitySensor::setSlotHoldTime(const base::Time* const msg)
{
   if (msg == nullptr) return false;
   holdTimeSec = base::Seconds::convertStatic(*msg);
   return (holdTimeSec >= 0.0);
}

bool ProximitySensor::setSlotHostileOnly(const base::Number* const msg)
{
   if (msg == nullptr) return false;
   hostileOnly = (msg->getInt() != 0);
   return true;
}

} // namespace xdrone
} // namespace mixr
