#include "xnative/factory.hpp"

#include "Beacon.hpp"
#include "events/payloads/EID_PING/PingMessage.hpp"

#include "mixr/base/MetaObject.hpp"
#include "mixr/base/Object.hpp"

namespace mixr {
namespace models {
namespace xBeacon {

//------------------------------------------------------------------------------
// Registra as duas classes deste modelo. Ao acrescentar uma classe nova,
// ela entra em TRES lugares, sempre juntos: aqui (o if/else), em NOMES[] e
// em METAS[] logo abaixo -- o registro de plugin confere as tres listas na
// carga e recusa se ficarem fora de sincronia.
//
// 'PingMessage' entra aqui -- nao porque o .edl construa um por nome (ele
// so' nasce programaticamente, dentro de Beacon::broadcastPing()), mas no
// mesmo espirito de models/players/A-4 acrescentando 'TacticalAlert' ao
// seu: o payload aparece na aba Memoria do ./app como qualquer outra classe
// deste modelo, e 'provides:' descreve o conjunto INTEIRO que esta .so
// exporta.
//------------------------------------------------------------------------------
base::Object* factory(const std::string& name)
{
   base::Object* obj{};

   if (name == Beacon::getFactoryName())               obj = new Beacon();
   else if (name == events::PingMessage::getFactoryName()) obj = new events::PingMessage();

   return obj;
}

namespace {

const char* const NOMES[] = {
   "Beacon",
   "PingMessage",
   nullptr
};

const base::MetaObject* const METAS[] = {
   Beacon::getMetaObject(),
   events::PingMessage::getMetaObject(),
   nullptr
};

} // namespace

const char* const* factoryNames()             { return NOMES; }
const base::MetaObject* const* metaObjects()  { return METAS; }

} // namespace xBeacon
} // namespace models
} // namespace mixr
