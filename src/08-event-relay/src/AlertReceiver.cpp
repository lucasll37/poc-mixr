#include "AlertReceiver.hpp"
#include "events.hpp"

#include "mixr/models/player/Player.hpp"
#include "mixr/models/WorldModel.hpp"
#include "mixr/simulation/AbstractDataRecorder.hpp"
#include "mixr/simulation/recorder_macros.hpp"
#include "mixr/simulation/dataRecorderTokens.hpp"

#include <iostream>
#include <sstream>

namespace events {

IMPLEMENT_SUBCLASS(AlertReceiver, "AlertReceiver")
EMPTY_SLOTTABLE(AlertReceiver)
EMPTY_DELETEDATA(AlertReceiver)

AlertReceiver::AlertReceiver()
{
   STANDARD_CONSTRUCTOR()
}

void AlertReceiver::copyData(const AlertReceiver& org, const bool)
{
   BaseClass::copyData(org);
   // pendingAlert/numAlertsReceived sao estado de execucao, nao copiados.
}

BEGIN_EVENT_HANDLER(AlertReceiver)
   ON_EVENT_OBJ(CONTACT_EVENT, onContactEvent, RadarContactMessage)
END_EVENT_HANDLER()

bool AlertReceiver::onContactEvent(RadarContactMessage* const msg)
{
   numAlertsReceived += 1;

   const mixr::models::Player* const me{getOwnship()};
   const std::string myName{me != nullptr && me->getName() != nullptr ? me->getName()->getString() : "?"};

   std::ostringstream oss;
   oss << "Alert @" << myName << ": contact #" << msg->trackId << " (" << msg->targetName << ")"
       << " range=" << msg->rangeNM << "NM bearing=" << msg->bearingDeg << "deg"
       << " -- relayed by " << msg->sourcePlayerName;
   pendingAlert = oss.str();

   std::cout << "[AlertReceiver] " << pendingAlert << std::endl;

   // Grava o alerta no recorder NATIVO, com a mesma macro que o framework
   // usa nos seus proprios eventos. Dai ele sai em todas as saidas
   // configuradas no 'dataRecorder' (arquivo, CSV e Tacview) sem que o
   // main.cpp precise consumir nada -- e por isso consumePendingAlert()
   // deixou de ser chamado no laco principal.
   //
   // REID_MARKER e o token nativo para "marcador da aplicacao": V1 = id do
   // marcador, V2 = quem o originou. Nao carrega texto -- MarkerMsg so tem
   // dois uint32 -- entao o texto completo continua indo para o console.
   BEGIN_RECORD_DATA_SAMPLE( getWorldModel()->getDataRecorder(), REID_MARKER )
      SAMPLE_2_VALUES( static_cast<double>(msg->trackId),
                       static_cast<double>(me != nullptr ? me->getID() : 0) )
   END_RECORD_DATA_SAMPLE()

   return true;
}

std::string AlertReceiver::consumePendingAlert()
{
   std::string a;
   a.swap(pendingAlert);
   return a;
}

} // namespace events
