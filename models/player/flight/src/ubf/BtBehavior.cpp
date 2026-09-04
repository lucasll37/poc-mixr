//
// BtBehavior -- a DECISAO. Este arquivo trata do ciclo de vida da arvore e
// do tick que produz a acao. A configuracao vinda do EDL (slottable, slot
// map e os setters) mora em BtBehaviorSlots.cpp; os numeros que ela ajusta,
// em include/ubf/BtTuning.hpp.
//
#include "ubf/BtBehavior.hpp"

#include "bt/bt_factory.hpp"
#include "bt/bt_factory_sdk.hpp"
#include "ubf/FlightAction.hpp"
#include "xlog/Log.hpp"
#include "xrandom/DeterministicRng.hpp"

#include "mixr/models/player/Player.hpp"

#include <exception>
#include <mutex>

namespace mixr {
namespace xnative {

IMPLEMENT_SUBCLASS(BtBehavior, "BtBehavior")

EMPTY_DELETEDATA(BtBehavior)

namespace {
// createTreeFromFile() nao e reentrante e varios avioes chegam ao primeiro
// genAction() ao mesmo tempo, em threads T/C diferentes.
std::mutex g_treeBuildMutex;

// Salt de PROPOSITO da segunda derivacao (instancia -> gerador do jitter de
// patrulha) -- fixo e arbitrario, so precisa ser diferente de outros salts
// de proposito que apareçam no futuro (ex.: se RtbPlan ganhar variacao
// propria, deriva do MESMO instanceSeed com OUTRO salt, garantindo streams
// sem correlacao entre si). "PATROLJ" em ASCII, soh para ser memoravel.
constexpr std::uint64_t kPatrolJitterSalt{0x5041'5452'4F4C'4A00ULL};
}

BtBehavior::BtBehavior()
{
   STANDARD_CONSTRUCTOR()
}

void BtBehavior::copyData(const BtBehavior& org, const bool)
{
   BaseClass::copyData(org);

   tune = org.tune;

   // A arvore NAO e copiada (BT::Tree e move-only e cada aeronave precisa
   // da sua): a copia reconstroi no primeiro genAction().
   treeBuilt = false;
   treeValid = false;
   plansReady = false;
   snap = FlightState::Snapshot{};
   currentDecision.reset();
   threat.reset();
}

void BtBehavior::reset()
{
   BaseClass::reset();

   configurePlans();
   patrol.reset();
   plansReady = true;

   if (treeValid) tree.haltTree();
   tree = BT::Tree();
   treeBuilt = false;
   treeValid = false;
   currentDecision.reset();
}

bool BtBehavior::shutdownNotification()
{
   if (treeValid) tree.haltTree();
   tree = BT::Tree();
   treeValid = false;
   treeBuilt = true;
   return BaseClass::shutdownNotification();
}

//------------------------------------------------------------------------------
// configurePlans() -- traduz os numeros do EDL para as regras de negocio
// puras de domain/. E o unico ponto em que BtTuning encontra domain::*.
//
// E tambem o unico ponto em que a hierarquia de sementes de shared/xrandom
// e calculada (ver o cabecalho de DeterministicRng.hpp e a secao "shared/
// xrandom" do CLAUDE.md para o "porque" completo). Resumo: a sub-semente de
// cada player vem de um HASH DO PROPRIO NOME, nunca de ordem de descoberta
// ou de processamento entre players -- essa ordem nao e garantida neste
// framework (a poc multi-thread decide em paralelo, um player por thread do
// pool de tempo critico), e qualquer esquema baseado em ordem quebraria o
// determinismo entre 1/2/4 threads. patrolSeedOverride, quando declarado,
// pula so a derivacao por NOME -- a derivacao de PROPOSITO (kPatrolJitterSalt)
// continua acontecendo do mesmo jeito nos dois casos.
//------------------------------------------------------------------------------
void BtBehavior::configurePlans()
{
   patrol.configure(tune.patrolHeadingDeg, tune.legTimeSec, tune.legTurnDeg,
                    tune.patrolAltitudeM, tune.patrolSpeedKts);

   const auto player = static_cast<models::Player*>(findContainerByType(typeid(models::Player)));
   const char* const playerName = (player != nullptr && player->getName() != nullptr)
                                   ? player->getName()->getString() : "";
   const std::uint64_t instanceSeed = tune.patrolSeedOverrideSet
      ? tune.patrolSeedOverride
      : xrandom::deriveSeed(tune.patrolMasterSeed, xrandom::fnv1a64(playerName));
   patrol.setHeadingJitter(tune.patrolJitterHeadingDeg,
                            xrandom::deriveSeed(instanceSeed, kPatrolJitterSalt));

   rtb.configure(0.0, 0.0, tune.arrivalRadiusM, tune.rtbAltitudeM, tune.rtbSpeedKts);

   domain::EvasionLimits limits;
   limits.breakTurnDeg = tune.breakTurnDeg;
   limits.climbM = tune.evadeClimbM;
   limits.dashSpeedKts = tune.evadeSpeedKts;
   limits.holdSeconds = tune.evadeHoldSec;
   limits.terrainClearanceM = tune.terrainClearanceM;
   threat.setLimits(limits);
   threat.reset();
}

void BtBehavior::buildTree()
{
   treeBuilt = true;   // uma tentativa so

   if (tune.treeFile.empty()) {
      LOG(WARNING) << "[BtBehavior] slot 'treeFile' vazio -- arvore nao construida";
      return;
   }

   bt_nodes::NodeContext context;
   context.behavior = this;

   std::lock_guard<std::mutex> lock(g_treeBuildMutex);
   bt_nodes::registerNodes(btFactory, context);
   bt_nodes::registerSdkNodes(btFactory, context);

   try {
      tree = btFactory.createTreeFromFile(tune.treeFile, BT::Blackboard::create());
      treeValid = true;
   } catch (const std::exception& ex) {
      LOG(ERROR) << "[BtBehavior] falha ao carregar a arvore: " << ex.what();
      treeValid = false;
   }
}

//------------------------------------------------------------------------------
// feedThreatPolicy() -- a unica traducao Snapshot -> domain nesta classe.
//
// Roda ANTES do tick: quando os nos da arvore perguntarem "estou evadindo?",
// a politica ja terá visto o frame e envelhecido a histerese.
//------------------------------------------------------------------------------
void BtBehavior::feedThreatPolicy(const double dt)
{
   domain::ThreatContact contact;
   contact.rangeM = snap.contactRangeM;
   contact.relBearingDeg = snap.contactRelBearingDeg;
   contact.deltaAltM = snap.contactDeltaAltM;

   domain::GroundReference ground;
   ground.valid = snap.terrainValid;
   ground.elevationM = snap.terrainElevM;

   threat.update(dt, snap.hasContact, contact, snap.headingDeg, snap.altitudeM, ground);
}

//------------------------------------------------------------------------------
// genAction() -- chamado pelo Agent (fase 3), depois de updateState().
//------------------------------------------------------------------------------
base::ubf::AbstractAction* BtBehavior::genAction(const base::ubf::AbstractState* const state,
                                                 const double dt)
{
   const auto flightState = dynamic_cast<const FlightState*>(state);
   if (flightState == nullptr) return nullptr;

   // Ver a armadilha no cabecalho: reset() pode nunca chegar a um
   // comportamento aninhado no Arbiter, entao a configuracao vinda dos
   // slots e aplicada aqui, na primeira decisao.
   if (!plansReady) {
      configurePlans();
      patrol.reset();
      plansReady = true;
   }

   snap = flightState->snapshot();
   if (!snap.valid) return nullptr;

   frameDt = dt;
   feedThreatPolicy(dt);

   if (!treeBuilt) buildTree();
   if (!treeValid) return nullptr;

   currentDecision.reset();
   tree.tickRoot();
   if (!currentDecision.taken) return nullptr;

   // Acao PRE-REF'd (o Agent chama unref() depois de executar) -- contrato
   // do UBF: "returns a pre-ref'd Action".
   const auto action = new FlightAction();
   action->setCommand(currentDecision.command);
   action->setLabel(currentDecision.label);
   if (currentDecision.broadcastAlert) {
      action->setAlertBroadcast(currentDecision.alertContactName,
                                currentDecision.alertNorthM, currentDecision.alertEastM,
                                currentDecision.alertAltitudeM, currentDecision.alertRangeM);
   }
   if (currentDecision.launchRequested) {
      action->setLaunchRequest(currentDecision.launchTargetName);
   }

   // O voto do comportamento vai junto: e por ele que o UbfArbiter escolhe
   // entre esta acao e a de outro comportamento no mesmo frame.
   action->setVote(getVote());
   return action;
}

} // namespace xnative
} // namespace mixr
