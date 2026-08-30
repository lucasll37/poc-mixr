#include "xnative/factory.hpp"

#include "ubf/AltitudeSafetyBehavior.hpp"
#include "ubf/BtBehavior.hpp"
#include "ubf/FlightAction.hpp"
#include "ubf/FlightState.hpp"
#include "xnative/AlertDatalink.hpp"
#include "xnative/FlightAgentTC.hpp"
#include "xnative/TacticalAlert.hpp"

#include "mixr/base/Object.hpp"

namespace mixr {
namespace xnative {

//------------------------------------------------------------------------------
// Registra SETE classes proprias -- as mesmas seis do single-thread mais o
// FlightAgentTC, que e a unica diferenca entre os dois subprojetos. Nenhuma
// delas e player, dinamica, controle ou sensor: tudo isso continua vindo do
// framework (Aircraft/JSBSimModel/Autopilot/Antenna+Tws+AirTrkMgr/UbfArbiter).
//
// O que NAO da para herdar, e por que:
//   * FlightState/BtBehavior/AltitudeSafetyBehavior/FlightAction -- o UBF
//     define as INTERFACES de percepcao/decisao/atuacao, mas nao traz
//     implementacoes prontas (models/ so acrescenta SimAgent e
//     MultiActorAgent);
//   * FlightAgentTC -- o framework TEM a classe (base::ubf::AgentTC), mas
//     NENHUMA factory dele a constroi: base/factory.cpp registra apenas
//     'UbfAgent' e 'UbfArbiter'. Registrar um agente de tempo critico e,
//     portanto, obrigacao da aplicacao;
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
   else if ( name == FlightAgentTC::getFactoryName() )          obj = new FlightAgentTC();

   else if ( name == FlightState::getFactoryName() )            obj = new FlightState();
   else if ( name == BtBehavior::getFactoryName() )             obj = new BtBehavior();
   else if ( name == AltitudeSafetyBehavior::getFactoryName() ) obj = new AltitudeSafetyBehavior();
   else if ( name == FlightAction::getFactoryName() )           obj = new FlightAction();

   return obj;
}

} // namespace xnative
} // namespace mixr
