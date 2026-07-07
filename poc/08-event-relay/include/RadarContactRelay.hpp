#pragma once

#include "mixr/models/system/System.hpp"
#include "mixr/base/safe_ptr.hpp"

#include <set>

namespace mixr {
namespace base { class Identifier; }
namespace models { class TrackManager; }
}

namespace events {

//------------------------------------------------------------------------------
// Class: RadarContactRelay
//
// System anexado ao "hunter": a cada fase 3 (process(), mesma fase em que
// o proprio Radar nativo esvazia sua fila de relatorios pro TrackManager
// -- ver mixr::models::sensor::Radar::process()), consulta o TrackManager
// nativo e, pra cada pista NOVA, dispara um RadarContactMessage por dois
// caminhos, ambos usando o mecanismo NATIVO de evento
// (Component::event()/send()), nao chamada C++ direta de logica de
// aplicacao:
//
//   1) local:  send(localComponentName, CONTACT_EVENT, msg)  -- um "hop"
//      dentro da propria arvore de componentes do hunter (irmao).
//
//   2) remoto: acha outro Player pelo nome via WorldModel::getPlayers()
//      (mesma tecnica que mixr::models::system::Datalink::sendMessage()
//      usa pra "distribuir" uma mensagem entre players) e chama
//      player->send(componentName, CONTACT_EVENT, msg) nele -- o mesmo
//      send() nativo, agora enderecando um componente de OUTRO player.
//
// Slots:
//    trackManagerName      <Identifier>  nome do TrackManager a consultar
//    localComponentName    <Identifier>  componente irmao (mesmo player)
//    relayToPlayerName     <Identifier>  player de destino do relay
//    relayToComponentName  <Identifier>  componente no player de destino
//------------------------------------------------------------------------------
class RadarContactRelay final : public mixr::models::System
{
   DECLARE_SUBCLASS(RadarContactRelay, mixr::models::System)

public:
   RadarContactRelay();

protected:
   void process(const double dt) final;

private:
   bool setSlotTrackManagerName(const mixr::base::Identifier* const);
   bool setSlotLocalComponentName(const mixr::base::Identifier* const);
   bool setSlotRelayToPlayerName(const mixr::base::Identifier* const);
   bool setSlotRelayToComponentName(const mixr::base::Identifier* const);

   mixr::base::safe_ptr<const mixr::base::Identifier> trackManagerName;
   mixr::base::safe_ptr<const mixr::base::Identifier> localComponentName;
   mixr::base::safe_ptr<const mixr::base::Identifier> relayToPlayerName;
   mixr::base::safe_ptr<const mixr::base::Identifier> relayToComponentName;

   mixr::models::TrackManager* trackManager{}; // resolvido de forma preguicosa
   std::set<int> knownTrackIds;

   // send(id, event, Object*, SendData&) exige uma SendData por "canal" de
   // destino (cache do lookup por nome) -- uma pro hop local, uma pro hop
   // remoto.
   mixr::base::Component::SendData localSendData;
   mixr::base::Component::SendData remoteSendData;
};

} // namespace events
