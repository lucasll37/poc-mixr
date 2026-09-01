#include "xnative/factory.hpp"

#include "ubf/AltitudeSafetyBehavior.hpp"
#include "ubf/BtBehavior.hpp"
#include "ubf/FlightAction.hpp"
#include "ubf/FlightState.hpp"
#include "xnative/AlertDatalink.hpp"
#include "xnative/TacticalAlert.hpp"

#include "mixr/base/MetaObject.hpp"
#include "mixr/base/Object.hpp"

namespace mixr {
namespace xnative {

//------------------------------------------------------------------------------
// Registra SEIS classes proprias -- nenhuma delas e player, dinamica,
// controle ou sensor: tudo isso vem do framework
// (Aircraft/JSBSimModel/Autopilot/Antenna+Tws+AirTrkMgr/SimAgent/UbfArbiter).
//
// O que NAO da para herdar, e por que:
//   * FlightState/BtBehavior/AltitudeSafetyBehavior/FlightAction -- o UBF
//     define as INTERFACES de percepcao/decisao/atuacao, mas nao traz
//     implementacoes prontas (models/ so acrescenta SimAgent e
//     MultiActorAgent);
//   * AlertDatalink -- herda models::Datalink e so decide o que fazer com
//     a mensagem recebida;
//   * TacticalAlert -- a carga util do datalink e, por definicao, da
//     aplicacao.
//------------------------------------------------------------------------------
base::Object* factory(const std::string& name)
{
   base::Object* obj{};

   if ( name == AlertDatalink::getFactoryName() )               obj = new AlertDatalink();
   else if ( name == TacticalAlert::getFactoryName() )          obj = new TacticalAlert();

   else if ( name == FlightState::getFactoryName() )            obj = new FlightState();
   else if ( name == BtBehavior::getFactoryName() )             obj = new BtBehavior();
   else if ( name == AltitudeSafetyBehavior::getFactoryName() ) obj = new AltitudeSafetyBehavior();
   else if ( name == FlightAction::getFactoryName() )           obj = new FlightAction();

   return obj;
}

//------------------------------------------------------------------------------
// O que este modelo PUBLICA para o host, atraves do descritor do plugin.
//
// As duas listas tem de acompanhar o if/else acima -- o registro confere: se a
// fabrica devolver nulo para um nome declarado aqui, a carga e recusada
// dizendo que descritor e factory estao fora de sincronia.
//------------------------------------------------------------------------------
namespace {

const char* const NOMES[] = {
   "AlertDatalink",
   "TacticalAlert",
   "FlightState",
   "BtBehavior",
   "AltitudeSafetyBehavior",
   "FlightAction",
   nullptr
};

const base::MetaObject* const METAS[] = {
   AlertDatalink::getMetaObject(),
   TacticalAlert::getMetaObject(),
   FlightState::getMetaObject(),
   BtBehavior::getMetaObject(),
   AltitudeSafetyBehavior::getMetaObject(),
   FlightAction::getMetaObject(),
   nullptr
};

} // namespace

const char* const* factoryNames()             { return NOMES; }
const base::MetaObject* const* metaObjects()  { return METAS; }

} // namespace xnative
} // namespace mixr
