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

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <mutex>

namespace mixr {
namespace models {
namespace xA_4 {

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

// Salt de PROPOSITO do SEGUNDO consumidor do mesmo instanceSeed: o sorteio do
// intervalo entre acrobacias (domain::AerobaticPlan). E exatamente o caso que
// o comentario acima antecipa -- mesma semente de instancia, salt diferente,
// sequencias sem correlacao entre si. "ROLLSALT" em ASCII.
constexpr std::uint64_t kSlowRollSalt{0x524F'4C4C'5341'4C54ULL};

// Salt de PROPOSITO do TERCEIRO consumidor do mesmo instanceSeed: o atraso
// de reacao do piloto a uma ameaca de RWR (domain::EvasionReactionPlan).
// Mesmo raciocinio dos dois acima -- sequencia sem correlacao com o jitter
// de patrulha ou o sorteio de acrobacia, mesmo vindo do MESMO instanceSeed.
// "EVADELAY" em ASCII.
constexpr std::uint64_t kRwrReactionSalt{0x4556'4144'454C'4159ULL};
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
   rwrThreat.reset();
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

   // Segundo consumidor da MESMA hierarquia -- reaproveita o instanceSeed ja
   // calculado acima e deriva com o proprio salt de proposito. Nenhum slot de
   // semente novo: patrolMasterSeed e a semente do CENARIO (o nome e
   // historico), e e dela que todo gerador deste player descende.
   aerobatic.configure(tune.slowRollMinIntervalSec, tune.slowRollMaxIntervalSec,
                       tune.slowRollStick, tune.slowRollTimeoutSec);
   aerobatic.setSeed(xrandom::deriveSeed(instanceSeed, kSlowRollSalt));

   rtb.configure(0.0, 0.0, tune.arrivalRadiusM, tune.rtbAltitudeM, tune.rtbSpeedKts);

   launchEnvelope_.minRangeM = tune.launchMinRangeM;
   launchEnvelope_.maxRangeM = tune.launchMaxRangeM;
   launchEnvelope_.coneDeg = tune.launchConeDeg;

   domain::EvasionLimits limits;
   limits.breakTurnDeg = tune.breakTurnDeg;
   limits.climbM = tune.evadeClimbM;
   limits.dashSpeedKts = tune.evadeSpeedKts;
   limits.holdSeconds = tune.evadeHoldSec;
   limits.terrainClearanceM = tune.terrainClearanceM;
   threat.setLimits(limits);
   threat.reset();

   // Terceiro consumidor da MESMA hierarquia -- reaproveita 'instanceSeed'
   // mais uma vez, com o proprio salt de proposito. rwrThreat_ reusa a
   // MESMA geometria de manobra (limits) que a evasao por contato de radar
   // proprio ja usa: a forma da quebra (virar para longe, desconflitar em
   // altitude) nao muda com a origem da ameaca, so' o GATILHO muda.
   rwrReaction.configure(tune.evadeReactionMinDelaySec, tune.evadeReactionMaxDelaySec);
   rwrReaction.setSeed(xrandom::deriveSeed(instanceSeed, kRwrReactionSalt));
   rwrThreat.setLimits(limits);
   rwrThreat.reset();
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
   // 'reset()' zera 'treeBuilt' (permitindo um segundo 'buildTree()' na
   // mesma instancia) mas nao zerava 'btFactory'.
   // 'BT::BehaviorTreeFactory::registerBuilder()' lanca para qualquer ID ja
   // registrado -- um segundo 'registerNodes()'/'registerSdkNodes()' sobre
   // a mesma factory lancaria fora do try/catch, que so cobre
   // 'createTreeFromFile()'. Reatribuir uma factory nova a cada chamada
   // torna 'buildTree()' idempotente (mesmo padrao de 'tree = BT::Tree();'
   // em 'reset()'). Nao ha evidencia de que 'BtBehavior::reset()' seja
   // chamado uma segunda vez pelo cascade do 'UbfArbiter' nativo em
   // 'src/rl' hoje -- o fix e cautela defensiva, nao correcao de um crash
   // observado.
   btFactory = BT::BehaviorTreeFactory();
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
// a partir do ator recebido do Agent nativo), nao de
// 'findContainerByType(Player)' -- essa busca so funciona quando BtBehavior
// esta aninhado no proprio player (caso FlightAgentTC); devolveria vazio se
// BtBehavior estivesse dentro de um ( SimAgent ) da Station, onde a ligacao
// com o player e por nome (actorPlayerName:).
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

      // A faixa de UID entra no log de proposito: o contador de UID do
      // BT.CPP (tree_node.cpp) e 'static uint16_t uid = 1' por .so, nunca
      // reiniciado -- 'uid 1..N' indica que esta e a primeira arvore
      // construida naquele plugin; qualquer outra faixa indica o
      // contrario. Relevante para diagnosticar o SIGABRT do Groot 1.0.0
      // sem patch (ver secao Groot no CLAUDE.md raiz), que trata UID como
      // indice de deque.
      std::uint16_t minUid{0xFFFF};
      std::uint16_t maxUid{0};
      for (const auto& n : tree.nodes) {
         if (n == nullptr) continue;
         minUid = std::min(minUid, n->UID());
         maxUid = std::max(maxUid, n->UID());
      }
      // LOG(INFO) << "[BtBehavior] monitor do Groot ligado para " << snap.ownerName
      //           << " (tcp://*:1666 status, tcp://*:1667 topologia, "
      //           << tree.nodes.size() << " nos, uid " << minUid << ".." << maxUid << ")";
   } catch (const std::exception& ex) {
      LOG(WARNING) << "[BtBehavior] falha ao ligar o monitor do Groot: " << ex.what();
   }
}

namespace {
// Mesmo piso absoluto de domain::ThreatPolicy.cpp (MIN_SAFE_ALT_M) -- os
// dois soh valem quando NAO ha dado de elevacao (ver domain/TerrainFloor.hpp);
// duplicado aqui, nao promovido a constante compartilhada, porque
// domain::terrainFloorM()/clampToTerrain() ja recebem o piso como
// PARAMETRO de proposito (funcao pura, sem estado escondido) -- cada
// chamador continua dono do proprio numero, mesmo que hoje coincidam.
const double MIN_SAFE_ALT_M{200.0};
}

//------------------------------------------------------------------------------
// feedThreatPolicy() -- uma das duas traducoes Snapshot -> domain nesta
// classe (a outra e' clampAltitudeToTerrain(), logo abaixo).
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
// feedRwrEvasion() -- Snapshot -> domain::EvasionReactionPlan (o atraso
// estocastico) -> domain::ThreatPolicy (a manobra em si), nessa ordem.
//
// O PONTO CENTRAL: rwrThreat SO' recebe hasContact=true depois que
// rwrReaction.update() devolve true (o atraso ja' venceu, com a ameaca
// ainda presente) -- antes disso, rwrThreat.update() e' alimentada com
// 'false', exatamente como se nao houvesse ameaca nenhuma. E' isso que faz
// o piloto "demorar para reagir": a arvore (via RwrThreatDetectedCondition)
// so' enxerga rwrThreat.engaged() virar true no frame em que o atraso
// sorteado ja' se esgotou, nunca antes.
//------------------------------------------------------------------------------
void BtBehavior::feedRwrEvasion(const double dt)
{
   const bool reacting{rwrReaction.update(dt, snap.hasRwrThreat)};

   domain::ThreatContact contact;
   contact.rangeM = snap.rwrThreatRangeM;
   contact.relBearingDeg = snap.rwrThreatRelBearingDeg;
   contact.deltaAltM = snap.rwrThreatDeltaAltM;

   domain::GroundReference ground;
   ground.valid = snap.terrainValid;
   ground.elevationM = snap.terrainElevM;

   rwrThreat.update(dt, reacting, contact, snap.headingDeg, snap.altitudeM, ground);
}

//------------------------------------------------------------------------------
// clampAltitudeToTerrain() -- antes, so domain::ThreatPolicy aplicava o
// piso anti-CFIT de domain/TerrainFloor.hpp; RTB/SUPPORT/PATROL comandavam
// altitude sem nenhuma protecao. Reaproveita a mesma traducao
// Snapshot->GroundReference de feedThreatPolicy() para os nos que nao
// passam pela ThreatPolicy.
//------------------------------------------------------------------------------
double BtBehavior::clampAltitudeToTerrain(const double altitudeM) const
{
   domain::GroundReference ground;
   ground.valid = snap.terrainValid;
   ground.elevationM = snap.terrainElevM;

   return domain::clampToTerrain(altitudeM, ground, tune.terrainClearanceM, MIN_SAFE_ALT_M);
}

//------------------------------------------------------------------------------
// hasAerobaticAltitudeMargin() -- MESMA traducao Snapshot->GroundReference de
// clampAltitudeToTerrain() acima, mas responde uma pergunta diferente: nao
// "que altitude e' segura comandar agora", e sim "ha folga de sobra para
// COMECAR uma acrobacia" -- ver o comentario grande em bt/DecisionContext.hpp
// para o "porque". Reusa o MESMO MIN_SAFE_ALT_M de clampAltitudeToTerrain().
//------------------------------------------------------------------------------
bool BtBehavior::hasAerobaticAltitudeMargin() const
{
   // Negativo DESLIGA a borda (qualquer altitude "basta", inclusive abaixo
   // do proprio piso anti-CFIT) -- distinto de zero, que exige estar
   // NO MINIMO no piso, sem folga extra nenhuma.
   if (tune.slowRollMinMarginM < 0.0) return true;

   domain::GroundReference ground;
   ground.valid = snap.terrainValid;
   ground.elevationM = snap.terrainElevM;

   const double floorM{domain::terrainFloorM(ground, tune.terrainClearanceM, MIN_SAFE_ALT_M)};
   return (snap.altitudeM - floorM) >= tune.slowRollMinMarginM;
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
   feedRwrEvasion(dt);

   if (!treeBuilt) buildTree();
   if (!treeValid) return nullptr;

   currentDecision.reset();
   tree.tickRoot();
   if (!currentDecision.taken) return nullptr;

   // Acao PRE-REF'd (o Agent chama unref() depois de executar) -- contrato
   // do UBF: "returns a pre-ref'd Action".
   const auto action = new FlightAction();
#ifdef POC_LEAK_ONE_REF_PER_DECISION
   // Controle negativo do detector de vazamento (tests/memory/
   // check_leak_detector_controle_negativo.py) -- nunca entra no build de
   // producao, so em 'model_leak' sob a opcao 'variants'. Um ref() extra
   // aqui, nunca balanceado por 'unref()', reproduz de forma permanente o
   // defeito documentado em tests/README.md ("ref() a mais na
   // FlightAction"), garantindo que a suite 'memory' de fato detecta
   // vazamento.
   action->ref();
#endif
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

} // namespace xA_4
} // namespace models
} // namespace mixr
