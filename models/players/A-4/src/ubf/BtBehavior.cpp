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

#include <cstdlib>
#include <exception>
#include <mutex>

namespace mixr {
namespace models {
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

   // O monitor do Groot (se ligado) referencia a 'tree' antiga -- derrubar
   // ANTES dela sumir (mesmo motivo do reset()/shutdownNotification()
   // abaixo). Reconstroi sozinho no proximo buildTree(), se o env var ainda
   // apontar para o nome deste player.
   treePublisher_.reset();

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

   treePublisher_.reset();  // ver o comentario de copyData()
   if (treeValid) tree.haltTree();
   tree = BT::Tree();
   treeBuilt = false;
   treeValid = false;
   currentDecision.reset();
}

bool BtBehavior::shutdownNotification()
{
   treePublisher_.reset();  // ver o comentario de copyData()
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
// E tambem o unico ponto em que a hierarquia de sementes de libs/xrandom
// e calculada (ver o cabecalho de DeterministicRng.hpp e a secao "libs/
// xrandom" do CLAUDE.md para o "porque" completo). Resumo: a sub-semente de
// cada player vem de um HASH DO PROPRIO NOME, nunca de ordem de descoberta
// ou de processamento entre players -- essa ordem nao e garantida neste
// framework (os quatro players decidem em paralelo, um por thread do
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
   const char* const rawPlayerName = (player != nullptr) ? player->getName()->getString() : nullptr;
   const char* const playerName = (rawPlayerName != nullptr) ? rawPlayerName : "";
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
      startGrootMonitorIfRequested();
   } catch (const std::exception& ex) {
      LOG(ERROR) << "[BtBehavior] falha ao carregar a arvore: " << ex.what();
      treeValid = false;
   }
}

//------------------------------------------------------------------------------
// startGrootMonitorIfRequested() -- liga o modo Monitor do Groot (PublisherZMQ
// nativo do BT.CPP, ver a secao 'shared/xrlbridge'/'deps/groot' do CLAUDE.md)
// para ESTE player, se e so' se MIXR_GROOT_MONITOR (variavel de ambiente) for
// exatamente o nome dele. Desligado por padrao -- sem a variavel, nenhum
// custo, nenhuma porta aberta.
//
// O nome vem de 'snap.ownerName' (preenchido em FlightState::updateState(),
// que recebe o ator direto do Agent nativo) -- NAO de
// 'findContainerByType(Player)' a partir daqui. Medido rodando: essa busca
// funciona aqui (BtBehavior aninhado no proprio player, via FlightAgentTC)
// mas devolveria vazio se BtBehavior morasse dentro de um ( SimAgent )
// nativo, componente da Station -- a ligacao com o player seria por NOME
// via 'actorPlayerName:', nunca por container().
//
// So' um player por PROCESSO: o proprio PublisherZMQ lanca LogicError numa
// segunda instancia (so' faz sentido de qualquer forma -- cada player tem a
// sua 'tree', e so' ha uma janela do Groot olhando de cada vez). Portas
// 1666 (status) e 1667 (topologia) -- livres neste projeto (Tacview usa
// 1234-1239, DIS usa 3000-3005).
//------------------------------------------------------------------------------
void BtBehavior::startGrootMonitorIfRequested()
{
   const char* const target = std::getenv("MIXR_GROOT_MONITOR");
   if (target == nullptr || target[0] == '\0') return;
   if (snap.ownerName != target) return;

   try {
      treePublisher_ = std::make_unique<BT::PublisherZMQ>(tree);
      LOG(INFO) << "[BtBehavior] monitor do Groot ligado para " << snap.ownerName
                << " (tcp://*:1666 status, tcp://*:1667 topologia)";
   } catch (const std::exception& ex) {
      LOG(WARNING) << "[BtBehavior] falha ao ligar o monitor do Groot: " << ex.what();
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
#ifdef POC_LEAK_ONE_REF_PER_DECISION
   // CONTROLE NEGATIVO do detector de vazamento (tests/memory/
   // check_leak_detector_controle_negativo.py) -- NUNCA entra no build de
   // producao (so em 'model_leak', atras da opcao 'variants' em
   // models/players/A-4/meson.build). Um ref() a mais aqui, nunca balanceado
   // por um unref(), e o MESMO defeito ja provado manualmente uma vez (ver
   // tests/README.md, "ref() a mais na FlightAction") -- agora permanente,
   // pra provar que a suite 'memory' pegaria um vazamento de verdade.
   action->ref();
#endif
   action->setCommand(currentDecision.command);
   action->setLabel(currentDecision.label);
   if (currentDecision.broadcastAlert) {
      action->setAlertBroadcast(currentDecision.alertContactName,
                                currentDecision.alertNorthM, currentDecision.alertEastM,
                                currentDecision.alertAltitudeM, currentDecision.alertRangeM);
   }

   // O voto do comportamento vai junto: e por ele que o UbfArbiter escolhe
   // entre esta acao e a de outro comportamento no mesmo frame.
   action->setVote(getVote());
   return action;
}

} // namespace xnative
} // namespace models
} // namespace mixr
