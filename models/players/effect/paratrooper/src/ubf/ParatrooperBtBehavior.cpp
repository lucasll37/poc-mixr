#include "ubf/ParatrooperBtBehavior.hpp"

#include "ubf/ParatrooperAction.hpp"
#include "ubf/ParatrooperState.hpp"

#include "bt/bt_factory.hpp"

#include "xlog/Log.hpp"

#include "mixr/base/String.hpp"
#include "mixr/base/numeric/Number.hpp"
#include "mixr/base/units/Distances.hpp"

#include <mutex>

namespace mixr {
namespace models {
namespace xparatrooper {

namespace {

// BT::BehaviorTreeFactory NAO e' thread-safe na CONSTRUCAO da arvore, e um
// cenario com N paraquedistas constroi N arvores -- potencialmente em
// paralelo, ja que o agente decide na fase 3 do frame de tempo critico
// (( ParatrooperAgentTC )). Serializar so' a construcao e' barato: acontece
// uma vez por entidade, nunca por tick.
std::mutex g_treeBuildMutex;

} // namespace

IMPLEMENT_SUBCLASS(ParatrooperBtBehavior, "ParatrooperBtBehavior")

// clang-format off
BEGIN_SLOTTABLE(ParatrooperBtBehavior)
   "treeFile",        // 1
   "deployAgl",       // 2
   "groundAgl",       // 3
   "requireTerrain",  // 4
END_SLOTTABLE(ParatrooperBtBehavior)

BEGIN_SLOT_MAP(ParatrooperBtBehavior)
   ON_SLOT(1, setSlotTreeFile,       base::String)
   ON_SLOT(2, setSlotDeployAgl,      base::Distance)
   ON_SLOT(3, setSlotGroundAgl,      base::Distance)
   ON_SLOT(4, setSlotRequireTerrain, base::Number)
END_SLOT_MAP()

EMPTY_DELETEDATA(ParatrooperBtBehavior)

ParatrooperBtBehavior::ParatrooperBtBehavior()
// clang-format on
{
   STANDARD_CONSTRUCTOR()
}

void ParatrooperBtBehavior::copyData(const ParatrooperBtBehavior& org, const bool)
{
   BaseClass::copyData(org);
   stage_ = org.stage_;
   profile_ = org.profile_;
   requireTerrain_ = org.requireTerrain_;
   decision_ = org.decision_;
   treeFile_ = org.treeFile_;

   // A arvore NAO e' copiada: BT::Tree guarda os nos construidos, e cada no
   // carrega um NodeContext apontando para o comportamento que o hospeda --
   // copiar traria ponteiros para o OUTRO objeto.
   treeBuilt_ = false;
   treeValid_ = false;
   tree_ = BT::Tree();
}

void ParatrooperBtBehavior::reset()
{
   BaseClass::reset();
   stage_ = domain::Stage::FREEFALL;
   decision_.reset();

   // Derruba a arvore ANTES de permitir a reconstrucao -- 'tree_' guarda
   // referencias para o que 'btFactory_' produziu.
   tree_ = BT::Tree();
   treeBuilt_ = false;
   treeValid_ = false;
}

//------------------------------------------------------------------------------
// buildTree() -- uma tentativa so' por ciclo de vida (ou por reset()).
//
// Preguicoso, e nao no construtor/reset(), porque o slot 'treeFile' so' esta
// preenchido depois que o parser do EDL terminou de montar o objeto.
//------------------------------------------------------------------------------
void ParatrooperBtBehavior::buildTree()
{
   treeBuilt_ = true;

   if (treeFile_.empty()) {
      LOG(WARNING) << "[ParatrooperBtBehavior] slot 'treeFile' vazio -- arvore nao construida";
      return;
   }

   bt::NodeContext context;
   context.behavior = this;

   std::lock_guard<std::mutex> lock(g_treeBuildMutex);

   // Factory NOVA a cada tentativa: registerBuilder() lanca
   // "ID already registered" pra um ID ja presente, entao um segundo
   // buildTree() sobre a MESMA factory (possivel depois de reset()) morreria
   // no primeiro no.
   btFactory_ = BT::BehaviorTreeFactory();
   bt::registerNodes(btFactory_, context);

   try {
      tree_ = btFactory_.createTreeFromFile(treeFile_, BT::Blackboard::create());
      treeValid_ = true;
   } catch (const std::exception& ex) {
      LOG(ERROR) << "[ParatrooperBtBehavior] falha ao carregar a arvore '" << treeFile_
                 << "': " << ex.what();
      treeValid_ = false;
   }
}

//------------------------------------------------------------------------------
// genAction() -- chamado uma vez por ciclo de decisao pelo Agent do UBF.
//
// Devolver nullptr e' legitimo no UBF ("nao tenho nada a recomendar neste
// frame"), e e' o que acontece quando a percepcao ainda nao e' valida.
//------------------------------------------------------------------------------
base::ubf::AbstractAction* ParatrooperBtBehavior::genAction(const base::ubf::AbstractState* const state,
                                                            const double)
{
   const auto s = dynamic_cast<const ParatrooperState*>(state);
   if (s == nullptr || !s->hasReading()) return nullptr;

   // O AVANCO da FSM acontece AQUI, uma vez por ciclo -- nunca dentro de um
   // no da arvore (ver bt/DecisionContext.hpp). Com o gate de terreno
   // ligado, o primeiro ciclo (antes do primeiro updateData() de fundo
   // validar 'tElev') mantem o estagio onde estava, em vez de reagir a uma
   // AGL que ainda e' so a altitude MSL crua.
   if (!requireTerrain_ || s->isTerrainValid()) {
      stage_ = domain::next(stage_, s->getAglM(), profile_);
   }

   if (!treeBuilt_) buildTree();

   decision_.reset();

   if (treeValid_) {
      tree_.tickRoot();
   } else {
      // DEGRADACAO, nao falha: sem arvore carregada o modelo continua
      // decidindo pela MESMA regra pura, direto -- e' o que mantem a
      // entidade decidindo (e o cenario rodando) quando o .xml some do
      // lugar ou nao parseia.
      decision_.take(domain::labelOf(stage_));
   }

   const auto action = new ParatrooperAction(decision_.label);   // pre-ref'd: o Agent da unref()
   action->setVote(getVote());
   return action;
}

bool ParatrooperBtBehavior::setSlotTreeFile(const base::String* const msg)
{
   // getString(), nao c_str(): base::String guarda um 'char*' PRIVADO e
   // expoe so' este getter (mais um operator const char*). Pode voltar
   // nullptr num objeto default-construido.
   if (msg == nullptr || msg->getString() == nullptr) return false;
   treeFile_ = msg->getString();
   return true;
}

bool ParatrooperBtBehavior::setSlotDeployAgl(const base::Distance* const msg)
{
   if (msg == nullptr) return false;
   profile_.deployAglM = base::Meters::convertStatic(*msg);
   return true;
}

bool ParatrooperBtBehavior::setSlotGroundAgl(const base::Distance* const msg)
{
   if (msg == nullptr) return false;
   profile_.groundAglM = base::Meters::convertStatic(*msg);
   return true;
}

bool ParatrooperBtBehavior::setSlotRequireTerrain(const base::Number* const msg)
{
   if (msg == nullptr) return false;
   requireTerrain_ = msg->getBoolean();
   return true;
}

} // namespace xparatrooper
} // namespace models
} // namespace mixr
