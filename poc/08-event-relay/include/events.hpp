#pragma once

#include "mixr/base/Object.hpp"

#include <string>

namespace events {

// Evento customizado desta PoC. Convencao nativa do MIXR (ver
// mixr/include/mixr/base/eventTokens.hpp): eventos nao-de-tecla proprios
// da aplicacao devem comecar em USER_EVENTS (2000) pra nao colidir com os
// eventos reservados do framework (RESET_EVENT=1301, SHUTDOWN_EVENT=1001,
// DATALINK_MESSAGE=1310 etc).
const int CONTACT_EVENT{2001};

//------------------------------------------------------------------------------
// Class: RadarContactMessage
//
// Payload de dados de um contato de radar, transportado como o Object*
// de um Component::event()/send() -- mesmo mecanismo que o framework usa
// pra emissions de RF, mensagens de datalink, etc (ver Player::event()
// tratando RF_EMISSION/DATALINK_MESSAGE com um Object* de payload).
//------------------------------------------------------------------------------
class RadarContactMessage final : public mixr::base::Object
{
   DECLARE_SUBCLASS(RadarContactMessage, mixr::base::Object)

public:
   RadarContactMessage(const int trackId, const std::string& targetName,
                        const double rangeNM, const double bearingDeg,
                        const std::string& sourcePlayerName);

   int trackId{};
   std::string targetName;
   double rangeNM{};
   double bearingDeg{};
   std::string sourcePlayerName;
};

} // namespace events
