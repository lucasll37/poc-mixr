//
// O MIRROR DE CONTRATO -- segundo artefato deste projeto, NAO faz parte do
// scaffold copiavel.
//
// ==============================================================================
// ATENCAO, QUEM COPIAR ESTE DIRETORIO PARA COMECAR UM MODELO NOVO:
// apague este arquivo (src/mirror.cpp) e o bloco 'template_mirror_lib' de
// meson.build/tests/meson.build ANTES de personalizar a copia. Um modelo
// novo que continuasse exportando os 9 nomes abaixo colidiria de fabrica
// com models/players/A-4 -- o mesmo incidente real ja documentado em
// CLAUDE.md ("ThreadTagProbe, A-4 x missile"). Ver docs/PRIMEIROS-PASSOS.md.
// ==============================================================================
//
// Escrito SO contra o SDK publicado. Nao inclui, nao le e nao conhece nada
// de models/players/A-4 -- so:
//    dist/include/xplugin/PluginAbi.hpp   (o contrato de empacotamento)
//    dist/include/xboard/Board.hpp        (o quadro de leitura)
//    o cenario de producao                (que nomes e que slots usar)
//    os headers do MIXR
//
// POR QUE ELE EXISTE. Os outros testes de plugin provam que o mecanismo de
// carga funciona -- mas todos carregam O MESMO modelo, compilado do mesmo
// fonte. Nenhum deles pode falhar por "um .so desconhecido nao serve". Este
// pode: se o contrato nao bastar, ele nao carrega, ou carrega e o dump sai
// errado. Herdou este papel de models/players/fixtures/stub (removido) --
// ver models/template/docs/CONTRATO.md.
//
// O comportamento e deliberadamente trivial: voa reto e nivelado. O que se
// verifica aqui NAO e voo bonito, e que a aplicacao INTEIRA sobe, parseia o
// cenario de producao e roda com um modelo que ela nunca viu.
//
#include "xplugin/PluginAbi.hpp"
#include "xboard/Board.hpp"

#include "mixr/base/Component.hpp"
#include "mixr/base/Pair.hpp"
#include "mixr/base/String.hpp"
#include "mixr/base/numeric/Number.hpp"
#include "mixr/base/ubf/AbstractAction.hpp"
#include "mixr/base/ubf/AbstractBehavior.hpp"
#include "mixr/base/ubf/AbstractState.hpp"
#include "mixr/base/ubf/Agent.hpp"
#include "mixr/base/units/Angles.hpp"
#include "mixr/base/units/Distances.hpp"
#include "mixr/base/units/Times.hpp"
#include "mixr/models/WorldModel.hpp"
#include "mixr/models/player/Player.hpp"
#include "mixr/models/system/Autopilot.hpp"
#include "mixr/models/system/Datalink.hpp"
#include "mixr/models/system/Gimbal.hpp"
#include "mixr/models/system/ScanGimbal.hpp"
#include "mixr/models/player/air/AirVehicle.hpp"

#include <cstring>
#include <string>

namespace mixr {
namespace models {
namespace xtemplate_mirror {

//------------------------------------------------------------------------------
// Percepcao -- o cenario a declara em 'state: ( FlightState )'. Sem slots.
//------------------------------------------------------------------------------
class FlightState final : public base::ubf::AbstractState
{
   DECLARE_SUBCLASS(FlightState, base::ubf::AbstractState)
public:
   FlightState()                                        { STANDARD_CONSTRUCTOR() }

   // A percepcao deste modelo e trivial -- mas ela PUBLICA a varredura de
   // radar, porque essa e uma obrigacao do modelo (ver CONTRATO.md).
   void updateState(const base::Component* const actor) override
   {
      const auto air = dynamic_cast<const models::AirVehicle*>(actor);
      if (air == nullptr) return;

      models::Gimbal* const gimbal{const_cast<models::AirVehicle*>(air)->getGimbalByName("radar")};
      if (gimbal == nullptr) {
         xboard::setRadarScan(air->getID(), false, 0.0, 0.0, 0.0, 0.0, 0.0);
         return;
      }
      double h{}, v{};
      if (const auto* const sg = dynamic_cast<const models::ScanGimbal*>(gimbal)) {
         sg->getScanVolumeD(&h, &v);
      }
      xboard::setRadarScan(air->getID(), true, gimbal->getAzimuthD(), gimbal->getElevationD(),
                           gimbal->getMaxRange2PlayersOfInterest(), h, v);
   }
};
IMPLEMENT_SUBCLASS(FlightState, "FlightState")
EMPTY_SLOTTABLE(FlightState)
EMPTY_DELETEDATA(FlightState)
EMPTY_COPYDATA(FlightState)

//------------------------------------------------------------------------------
// Atuacao -- comanda voo reto e nivelado E ESCREVE NO QUADRO.
//
// A escrita no xboard e a obrigacao mais facil de esquecer, e a unica que
// falha em SILENCIO: sem ela o host imprime 'bt=--' e 'dec=0' em todas as
// linhas, sem erro de carga, sem aviso, e todos os outros testes ficam verdes.
//------------------------------------------------------------------------------
class FlightAction final : public base::ubf::AbstractAction
{
   DECLARE_SUBCLASS(FlightAction, base::ubf::AbstractAction)
public:
   FlightAction()                                       { STANDARD_CONSTRUCTOR() }
   bool execute(base::Component* actor) override;
};
IMPLEMENT_SUBCLASS(FlightAction, "FlightAction")
EMPTY_SLOTTABLE(FlightAction)
EMPTY_DELETEDATA(FlightAction)
EMPTY_COPYDATA(FlightAction)

bool FlightAction::execute(base::Component* const actor)
{
   const auto player = dynamic_cast<models::Player*>(actor);
   if (player == nullptr) return false;

   base::Pair* const pilotPair{player->getPilotByType(typeid(models::Autopilot))};
   const auto autopilot = (pilotPair != nullptr)
                           ? dynamic_cast<models::Autopilot*>(pilotPair->object())
                           : nullptr;
   if (autopilot != nullptr) {
      autopilot->setHeadingHoldMode(true);
      autopilot->setAltitudeHoldMode(true);
      autopilot->setVelocityHoldMode(true);
      // Rumo e altitude ATUAIS -- voa reto e nivelado de onde estiver.
      autopilot->setCommandedHeadingD(player->getHeadingD());
      autopilot->setCommandedAltitudeFt(player->getAltitudeM() * 3.280839895013123);
      autopilot->setCommandedVelocityKts(160.0);
   }

   // As DUAS chamadas obrigatorias. Ver CONTRATO.md.
   xboard::setBehaviorLabel(player->getID(), "PATROL");
   xboard::bumpDecisionCount(player->getID());
   return true;
}

//------------------------------------------------------------------------------
// Decisao -- o cenario declara todos os slots abaixo. Um slot que o modelo
// nao conheca faz o parser somar erro e o host abortar, entao a tabela tem
// de cobrir o cenario inteiro.
//------------------------------------------------------------------------------
class BtBehavior final : public base::ubf::AbstractBehavior
{
   DECLARE_SUBCLASS(BtBehavior, base::ubf::AbstractBehavior)
public:
   BtBehavior()                                         { STANDARD_CONSTRUCTOR() }
   base::ubf::AbstractAction* genAction(const base::ubf::AbstractState* const,
                                        const double) override
   {
      const auto a = new FlightAction();   // pre-ref'd: o Agent da unref()
      a->setVote(getVote());
      return a;
   }
private:
   // Aceita e ignora. O mirror nao voa a arvore -- so precisa nao recusar o
   // cenario de producao.
   bool setSlotIgnoraString(const base::String* const x)    { return x != nullptr; }
   bool setSlotIgnoraNumero(const base::Number* const x)    { return x != nullptr; }
   bool setSlotIgnoraAngulo(const base::Angle* const x)     { return x != nullptr; }
   bool setSlotIgnoraDistancia(const base::Distance* const x){ return x != nullptr; }
   bool setSlotIgnoraTempo(const base::Time* const x)       { return x != nullptr; }
};
IMPLEMENT_SUBCLASS(BtBehavior, "BtBehavior")
EMPTY_DELETEDATA(BtBehavior)
EMPTY_COPYDATA(BtBehavior)

// clang-format off
BEGIN_SLOTTABLE(BtBehavior)
   "treeFile", "patrolHeading", "legTime", "legTurn", "patrolAltitude",
   "patrolSpeed", "rtbAltitude", "rtbSpeed", "arrivalRadius", "fuelReserve",
   "breakTurn", "evadeClimb", "evadeSpeed", "supportSpeed", "evadeHold",
   "terrainClearance", "patrolJitterHeading", "patrolMasterSeed",
   "patrolSeedOverride",
END_SLOTTABLE(BtBehavior)

BEGIN_SLOT_MAP(BtBehavior)
   ON_SLOT( 1, setSlotIgnoraString,    base::String)
   ON_SLOT( 2, setSlotIgnoraAngulo,    base::Angle)
   ON_SLOT( 3, setSlotIgnoraTempo,     base::Time)
   ON_SLOT( 4, setSlotIgnoraAngulo,    base::Angle)
   ON_SLOT( 5, setSlotIgnoraDistancia, base::Distance)
   ON_SLOT( 6, setSlotIgnoraNumero,    base::Number)
   ON_SLOT( 7, setSlotIgnoraDistancia, base::Distance)
   ON_SLOT( 8, setSlotIgnoraNumero,    base::Number)
   ON_SLOT( 9, setSlotIgnoraDistancia, base::Distance)
   ON_SLOT(10, setSlotIgnoraNumero,    base::Number)
   ON_SLOT(11, setSlotIgnoraAngulo,    base::Angle)
   ON_SLOT(12, setSlotIgnoraDistancia, base::Distance)
   ON_SLOT(13, setSlotIgnoraNumero,    base::Number)
   ON_SLOT(14, setSlotIgnoraNumero,    base::Number)
   ON_SLOT(15, setSlotIgnoraTempo,     base::Time)
   ON_SLOT(16, setSlotIgnoraDistancia, base::Distance)
   ON_SLOT(17, setSlotIgnoraAngulo,    base::Angle)
   ON_SLOT(18, setSlotIgnoraNumero,    base::Number)
   // ACHADO POR AUDITORIA, CORRIGIDO (nao redescobrir): 'patrolSeedOverride'
   // (slot 19 de producao, models/players/A-4/src/ubf/BtBehaviorSlots.cpp)
   // faltava aqui -- o mirror parava em 18 slots. Confirmado via
   // 'plugininfo' que essa era a UNICA diferenca entre libflight.so e
   // libtemplate_mirror.so. Inofensivo enquanto nenhum .edl de producao usa
   // esse slot (check_falcons_estrutura.sh exige o MESMO esqueleto nos 4
   // falcons, entao um slot so num deles ja quebraria essa guarda antes de
   // chegar aqui) -- mas o dia em que ele entrar em uso, sem este ON_SLOT
   // 'plugin-modelo-estranho'/'plugin-deposito-terceiro' quebrariam com
   // "slot not found" contra o mirror.
   ON_SLOT(19, setSlotIgnoraNumero,    base::Number)
END_SLOT_MAP()

class AltitudeSafetyBehavior final : public base::ubf::AbstractBehavior
// clang-format on
{
   DECLARE_SUBCLASS(AltitudeSafetyBehavior, base::ubf::AbstractBehavior)
public:
   AltitudeSafetyBehavior()                             { STANDARD_CONSTRUCTOR() }
   base::ubf::AbstractAction* genAction(const base::ubf::AbstractState* const,
                                        const double) override { return nullptr; }
private:
   bool setSlotIgnoraNumero(const base::Number* const x)     { return x != nullptr; }
   bool setSlotIgnoraDistancia(const base::Distance* const x){ return x != nullptr; }
};
IMPLEMENT_SUBCLASS(AltitudeSafetyBehavior, "AltitudeSafetyBehavior")
EMPTY_DELETEDATA(AltitudeSafetyBehavior)
EMPTY_COPYDATA(AltitudeSafetyBehavior)

// clang-format off
BEGIN_SLOTTABLE(AltitudeSafetyBehavior)
   "minAltitude", "recoverAltitude", "recoverSpeed", "minClearance", "recoverClearance",
END_SLOTTABLE(AltitudeSafetyBehavior)

BEGIN_SLOT_MAP(AltitudeSafetyBehavior)
   ON_SLOT(1, setSlotIgnoraDistancia, base::Distance)
   ON_SLOT(2, setSlotIgnoraDistancia, base::Distance)
   ON_SLOT(3, setSlotIgnoraNumero,    base::Number)
   ON_SLOT(4, setSlotIgnoraDistancia, base::Distance)
   ON_SLOT(5, setSlotIgnoraDistancia, base::Distance)
END_SLOT_MAP()

//------------------------------------------------------------------------------
// RLBridgeBehavior -- so precisa existir e se recusar a decidir (nullptr):
// nenhum cenario de teste que carrega o mirror liga a ponte de RL de verdade
// (isso exigiria libs/xrlbridge, que o mirror nao linka), so o cenario de
// PRODUCAO declara RLBridgeBehavior no seu 'provides:' -- e o mirror,
// rodando esse MESMO cenario trocando so o 'file:', precisa responder pelo
// nome pra nao quebrar a igualdade exata de conjunto.
//------------------------------------------------------------------------------
class RLBridgeBehavior final : public base::ubf::AbstractBehavior
// clang-format on
{
   DECLARE_SUBCLASS(RLBridgeBehavior, base::ubf::AbstractBehavior)
public:
   RLBridgeBehavior()                                    { STANDARD_CONSTRUCTOR() }
   base::ubf::AbstractAction* genAction(const base::ubf::AbstractState* const,
                                        const double) override { return nullptr; }
};
IMPLEMENT_SUBCLASS(RLBridgeBehavior, "RLBridgeBehavior")
EMPTY_SLOTTABLE(RLBridgeBehavior)
EMPTY_DELETEDATA(RLBridgeBehavior)
EMPTY_COPYDATA(RLBridgeBehavior)

//------------------------------------------------------------------------------
// ThreadTagProbe -- so precisa existir: nenhum cenario de teste que carrega o
// mirror declara ( ThreadTagProbe ) dentro de components: { }. O cenario de
// PRODUCAO declara o nome no seu 'provides:' porque o .so exporta a classe
// incondicionalmente -- o mirror, rodando esse MESMO cenario trocando so o
// 'file:', precisa responder pelo nome. Mesmo raciocinio de RLBridgeBehavior.
//------------------------------------------------------------------------------
class ThreadTagProbe final : public base::Component
{
   DECLARE_SUBCLASS(ThreadTagProbe, base::Component)
public:
   ThreadTagProbe()                                      { STANDARD_CONSTRUCTOR() }
};
IMPLEMENT_SUBCLASS(ThreadTagProbe, "ThreadTagProbe")
EMPTY_SLOTTABLE(ThreadTagProbe)
EMPTY_DELETEDATA(ThreadTagProbe)
EMPTY_COPYDATA(ThreadTagProbe)

//------------------------------------------------------------------------------
// FlightAgentTC -- o agente de tempo critico. So precisa existir e nao
// decidir errado: mesmas TRES armadilhas do framework que models/players/A-4/
// src/xnative/FlightAgentTC.cpp resolve (ver o cabecalho de lá para o
// "porque" completo de cada uma), replicadas aqui em miniatura para nao
// decidir 8x por frame por engano (4 fases x 2 caminhos, T/C e background).
//------------------------------------------------------------------------------
class FlightAgentTC final : public base::ubf::AgentTC
{
   DECLARE_SUBCLASS(FlightAgentTC, base::ubf::AgentTC)
public:
   FlightAgentTC()                                       { STANDARD_CONSTRUCTOR() }

   void updateData(const double) override                { /* no-op: ver cabecalho */ }

protected:
   void controller(const double dt) override
   {
      if (dt <= 0.0) return;
      if (getActor() == nullptr) initActor();

      const auto player = dynamic_cast<models::Player*>(getActor());
      if (player == nullptr) return;

      const models::WorldModel* const world{player->getWorldModel()};
      if (world == nullptr || world->phase() != 3) return;

      BaseClass::controller(dt * 4.0);
   }

   void initActor() override
   {
      if (getActor() != nullptr) return;
      const auto player = static_cast<models::Player*>(findContainerByType(typeid(models::Player)));
      if (player != nullptr) setActor(player);
   }
};
IMPLEMENT_SUBCLASS(FlightAgentTC, "FlightAgentTC")
EMPTY_SLOTTABLE(FlightAgentTC)
EMPTY_DELETEDATA(FlightAgentTC)
EMPTY_COPYDATA(FlightAgentTC)

//------------------------------------------------------------------------------
// A carga util do datalink e o datalink. O cenario poe ( AlertDatalink ) no
// slot 'datalink:' de cada aviao, entao ele TEM de derivar de models::Datalink
// -- Player::processComponents casa por findByType(typeid(models::Datalink)).
//------------------------------------------------------------------------------
class TacticalAlert final : public base::Object
{
   DECLARE_SUBCLASS(TacticalAlert, base::Object)
public:
   TacticalAlert()                                      { STANDARD_CONSTRUCTOR() }
};
IMPLEMENT_SUBCLASS(TacticalAlert, "TacticalAlert")
EMPTY_SLOTTABLE(TacticalAlert)
EMPTY_DELETEDATA(TacticalAlert)
EMPTY_COPYDATA(TacticalAlert)

class AlertDatalink final : public models::Datalink
{
   DECLARE_SUBCLASS(AlertDatalink, models::Datalink)
public:
   AlertDatalink()                                      { STANDARD_CONSTRUCTOR() }
   void receive(const double dt) override
   {
      BaseClass::receive(dt);
      const auto owner = static_cast<const models::Player*>(findContainerByType(typeid(models::Player)));
      if (owner != nullptr) {
         // Sem alerta nenhum, mas PUBLICA -- e o que faz o dump dizer
         // 'alert=none sent=0 recv=0' em vez de nao dizer nada.
         xboard::setAlert(owner->getID(), false, "", "");
         xboard::setDatalinkCounters(owner->getID(), 0, 0);
      }
   }
private:
   bool setSlotIgnoraTempo(const base::Time* const x)   { return x != nullptr; }
};
IMPLEMENT_SUBCLASS(AlertDatalink, "AlertDatalink")
EMPTY_DELETEDATA(AlertDatalink)
EMPTY_COPYDATA(AlertDatalink)

// clang-format off
BEGIN_SLOTTABLE(AlertDatalink)
   "holdTime",
END_SLOTTABLE(AlertDatalink)
BEGIN_SLOT_MAP(AlertDatalink)
   ON_SLOT(1, setSlotIgnoraTempo, base::Time)
END_SLOT_MAP()

//------------------------------------------------------------------------------
// A fronteira C
//------------------------------------------------------------------------------
// clang-format on
namespace {

base::Object* fabrica(const char* const name)
{
   if (name == nullptr) return nullptr;
   if (std::strcmp(name, "FlightState") == 0)            return new FlightState();
   if (std::strcmp(name, "FlightAction") == 0)           return new FlightAction();
   if (std::strcmp(name, "BtBehavior") == 0)             return new BtBehavior();
   if (std::strcmp(name, "AltitudeSafetyBehavior") == 0) return new AltitudeSafetyBehavior();
   if (std::strcmp(name, "RLBridgeBehavior") == 0)       return new RLBridgeBehavior();
   if (std::strcmp(name, "FlightAgentTC") == 0)          return new FlightAgentTC();
   if (std::strcmp(name, "TacticalAlert") == 0)          return new TacticalAlert();
   if (std::strcmp(name, "AlertDatalink") == 0)          return new AlertDatalink();
   if (std::strcmp(name, "ThreadTagProbe") == 0)         return new ThreadTagProbe();
   return nullptr;
}

const char* const NOMES[] = {
   "AlertDatalink", "TacticalAlert", "ThreadTagProbe", "FlightAgentTC", "FlightState",
   "BtBehavior", "AltitudeSafetyBehavior", "RLBridgeBehavior", "FlightAction",
   nullptr
};

const base::MetaObject* const METAS[] = {
   AlertDatalink::getMetaObject(), TacticalAlert::getMetaObject(),
   ThreadTagProbe::getMetaObject(), FlightAgentTC::getMetaObject(),
   FlightState::getMetaObject(), BtBehavior::getMetaObject(),
   AltitudeSafetyBehavior::getMetaObject(), RLBridgeBehavior::getMetaObject(),
   FlightAction::getMetaObject(),
   nullptr
};

} // namespace
} // namespace xtemplate_mirror
} // namespace models
} // namespace mixr

MIXR_PLUGIN_DEFINE("template_mirror",
                   mixr::models::xtemplate_mirror::fabrica,
                   mixr::models::xtemplate_mirror::NOMES,
                   mixr::models::xtemplate_mirror::METAS)
