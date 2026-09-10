#include "xnative/factory.hpp"

#include "ubf/BtBehavior.hpp"
#include "ubf/FlightAction.hpp"
#include "ubf/FlightState.hpp"
#include "xnative/ActionParatrooperRelease.hpp"
#include "xnative/ActionParatrooperStick.hpp"
#include "xnative/FlightAgentTC.hpp"
#include "xnative/ParatrooperPlaceholder.hpp"

#include "mixr/base/MetaObject.hpp"
#include "mixr/base/Object.hpp"

namespace mixr {
namespace models {
namespace xC_130 {

//------------------------------------------------------------------------------
// Registra as sete classes proprias deste modelo. FlightAgentTC e o UNICO
// agente -- decide sempre na fase 3 do frame de tempo critico, componente do
// proprio Player. ActionParatrooperRelease (um disparo por cruzamento)/
// ActionParatrooperStick (varios disparos, espacados no tempo, a partir de UM
// cruzamento -- ver o cabecalho dela para o mecanismo de Action::process())/
// ParatrooperPlaceholder sao a capacidade de largada, tratada como liberacao
// de ARMA (ver o cabecalho de cada uma). Nenhuma delas e player, dinamica,
// controle ou sensor -- tudo isso vem do framework (Aircraft/JSBSimModel/
// Autopilot/Navigation/Route/Steerpoint/StoresMgr).
//
// Nomes de fabrica com prefixo "C130" DE PROPOSITO (nao "FlightState"/
// "BtBehavior"/... como models/players/A-4): tests/guard/check_colisao_fabrica.py
// compara nome de fabrica par a par entre TODOS os modelos sob models/,
// independente de alguma vez serem carregados juntos no mesmo .edl --
// reaproveitar os nomes da A-4 derrubaria essa guarda na hora (mesma classe
// de incidente ja documentada para ThreadTagProbe, A-4 x o extinto modelo
// missile).
//------------------------------------------------------------------------------
base::Object* factory(const std::string& name)
{
   base::Object* obj{};

   if ( name == FlightAgentTC::getFactoryName() )               obj = new FlightAgentTC();
   else if ( name == ActionParatrooperRelease::getFactoryName() ) obj = new ActionParatrooperRelease();
   else if ( name == ActionParatrooperStick::getFactoryName() )   obj = new ActionParatrooperStick();
   else if ( name == ParatrooperPlaceholder::getFactoryName() )   obj = new ParatrooperPlaceholder();

   else if ( name == FlightState::getFactoryName() )            obj = new FlightState();
   else if ( name == BtBehavior::getFactoryName() )              obj = new BtBehavior();
   else if ( name == FlightAction::getFactoryName() )            obj = new FlightAction();

   return obj;
}

//------------------------------------------------------------------------------
// O que este modelo PUBLICA para o host, atraves do descritor do plugin.
//
// As duas listas tem de acompanhar o if/else acima -- o registro confere: se
// a fabrica devolver nulo para um nome declarado aqui, a carga e recusada
// dizendo que descritor e factory estao fora de sincronia.
//------------------------------------------------------------------------------
namespace {

const char* const NOMES[] = {
   "C130FlightAgentTC",
   "C130ActionParatrooperRelease",
   "C130ActionParatrooperStick",
   "C130ParatrooperPlaceholder",
   "C130FlightState",
   "C130BtBehavior",
   "C130FlightAction",
   nullptr
};

const base::MetaObject* const METAS[] = {
   FlightAgentTC::getMetaObject(),
   ActionParatrooperRelease::getMetaObject(),
   ActionParatrooperStick::getMetaObject(),
   ParatrooperPlaceholder::getMetaObject(),
   FlightState::getMetaObject(),
   BtBehavior::getMetaObject(),
   FlightAction::getMetaObject(),
   nullptr
};

} // namespace

const char* const* factoryNames()             { return NOMES; }
const base::MetaObject* const* metaObjects()  { return METAS; }

} // namespace xC_130
} // namespace models
} // namespace mixr
