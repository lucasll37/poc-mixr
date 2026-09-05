#include "xnative/factory.hpp"

#include "ubf/AltitudeSafetyBehavior.hpp"
#include "ubf/BtBehavior.hpp"
#include "ubf/FlightAction.hpp"
#include "ubf/FlightState.hpp"
#include "ubf/RLBridgeBehavior.hpp"
#include "xnative/AlertDatalink.hpp"
#include "xnative/ThreadTagProbe.hpp"
#ifdef FLIGHT_TC_AGENT
   #include "xnative/FlightAgentTC.hpp"
#endif
#include "events/payloads/EID_ALERT/TacticalAlert.hpp"

#include "mixr/base/MetaObject.hpp"
#include "mixr/base/Object.hpp"

namespace mixr {
namespace xnative {

//------------------------------------------------------------------------------
// UMA arvore de fonte, DOIS artefatos.
//
// A unica diferenca entre o modelo da single-thread e o da multi-thread e o
// FlightAgentTC -- o agente de tempo critico. Em vez de duas copias da arvore
// inteira (era assim ate aqui: ~3.100 linhas duplicadas, sustentadas por um
// teste de guarda), ele fica atras de FLIGHT_TC_AGENT e o meson produz
// libflight.so e libflight_tc.so do mesmo fonte.
//
// Registra as classes proprias -- nenhuma delas e player, dinamica,
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
//     aplicacao. Mora em events/, na raiz (nao em xnative/) desde que passou
//     a ser reutilizavel por outros plugins -- ver events/README.md.
//------------------------------------------------------------------------------
base::Object* factory(const std::string& name)
{
   base::Object* obj{};

   if ( name == AlertDatalink::getFactoryName() )                obj = new AlertDatalink();
   else if ( name == events::TacticalAlert::getFactoryName() )  obj = new events::TacticalAlert();
   else if ( name == ThreadTagProbe::getFactoryName() )         obj = new ThreadTagProbe();
#ifdef FLIGHT_TC_AGENT
   else if ( name == FlightAgentTC::getFactoryName() )          obj = new FlightAgentTC();
#endif

   else if ( name == FlightState::getFactoryName() )            obj = new FlightState();
   else if ( name == BtBehavior::getFactoryName() )             obj = new BtBehavior();
   else if ( name == AltitudeSafetyBehavior::getFactoryName() ) obj = new AltitudeSafetyBehavior();
   else if ( name == RLBridgeBehavior::getFactoryName() )       obj = new RLBridgeBehavior();
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
   "ThreadTagProbe",
#ifdef FLIGHT_TC_AGENT
   "FlightAgentTC",
#endif
   "FlightState",
   "BtBehavior",
   "AltitudeSafetyBehavior",
   "RLBridgeBehavior",
   "FlightAction",
   nullptr
};

const base::MetaObject* const METAS[] = {
   AlertDatalink::getMetaObject(),
   events::TacticalAlert::getMetaObject(),
   ThreadTagProbe::getMetaObject(),
#ifdef FLIGHT_TC_AGENT
   FlightAgentTC::getMetaObject(),
#endif
   FlightState::getMetaObject(),
   BtBehavior::getMetaObject(),
   AltitudeSafetyBehavior::getMetaObject(),
   RLBridgeBehavior::getMetaObject(),
   FlightAction::getMetaObject(),
   nullptr
};

} // namespace

const char* const* factoryNames()             { return NOMES; }
const base::MetaObject* const* metaObjects()  { return METAS; }

} // namespace xnative
} // namespace mixr
