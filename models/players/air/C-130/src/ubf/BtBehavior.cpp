//
// BtBehavior -- a DECISAO. Ciclo de vida da arvore (construcao preguicosa,
// uma tentativa por reset()) e o tick que produz a acao.
//
#include "ubf/BtBehavior.hpp"

#include "bt/bt_factory.hpp"
#include "ubf/FlightAction.hpp"

#include "xlog/Log.hpp"

#include "mixr/base/String.hpp"

#include <mutex>

namespace mixr {
namespace models {
namespace xC_130 {

IMPLEMENT_SUBCLASS(BtBehavior, "C130BtBehavior")

// clang-format off
BEGIN_SLOTTABLE(BtBehavior)
   "treeFile",   // 1
END_SLOTTABLE(BtBehavior)

BEGIN_SLOT_MAP(BtBehavior)
   ON_SLOT(1, setSlotTreeFile, base::String)
END_SLOT_MAP()

EMPTY_DELETEDATA(BtBehavior)

BtBehavior::BtBehavior()
// clang-format on
{
   STANDARD_CONSTRUCTOR()
}

void BtBehavior::copyData(const BtBehavior& org, const bool)
{
   BaseClass::copyData(org);
   treeFile_ = org.treeFile_;

   // A arvore NAO e copiada: BT::Tree guarda os nos construidos, e cada no
   // carrega um NodeContext apontando para o comportamento que o hospeda --
   // copiar traria ponteiros para o OUTRO objeto. Reconstruir e o unico
   // caminho correto.
   snap = FlightState::Snapshot{};
   currentDecision.reset();
   treeBuilt = false;
   treeValid = false;
   tree = BT::Tree();
}

void BtBehavior::reset()
{
   BaseClass::reset();

   currentDecision.reset();
   if (treeValid) tree.haltTree();
   tree = BT::Tree();
   treeBuilt = false;
   treeValid = false;
}

bool BtBehavior::shutdownNotification()
{
   if (treeValid) tree.haltTree();
   tree = BT::Tree();
   treeValid = false;
   treeBuilt = true;
   return BaseClass::shutdownNotification();
}

namespace {
// createTreeFromFile() nao e reentrante e varias aeronaves chegam ao
// primeiro genAction() ao mesmo tempo, em threads T/C diferentes.
std::mutex g_treeBuildMutex;
} // namespace

//------------------------------------------------------------------------------
// buildTree() -- uma tentativa so por ciclo de vida (ou por reset()).
//------------------------------------------------------------------------------
void BtBehavior::buildTree()
{
   treeBuilt = true;

   if (treeFile_.empty()) {
      LOG(WARNING) << "[BtBehavior] slot 'treeFile' vazio -- arvore nao construida";
      return;
   }

   bt::NodeContext context;
   context.behavior = this;

   std::lock_guard<std::mutex> lock(g_treeBuildMutex);

   // Factory NOVA a cada tentativa: registerBuilder() lanca para um ID ja
   // presente, entao um segundo buildTree() sobre a MESMA factory (possivel
   // depois de reset()) morreria no primeiro no.
   btFactory = BT::BehaviorTreeFactory();
   bt::registerNodes(btFactory, context);

   try {
      tree = btFactory.createTreeFromFile(treeFile_, BT::Blackboard::create());
      treeValid = true;
   } catch (const std::exception& ex) {
      LOG(ERROR) << "[BtBehavior] falha ao carregar a arvore '" << treeFile_
                 << "': " << ex.what();
      treeValid = false;
   }
}

//------------------------------------------------------------------------------
// genAction() -- chamado pelo Agent (fase 3), depois de updateState(). Sem
// nada a configurar preguicosamente (sem PatrolPlan/RtbPlan/ThreatPolicy) --
// so constroi a arvore na primeira vez e tica.
//------------------------------------------------------------------------------
base::ubf::AbstractAction* BtBehavior::genAction(const base::ubf::AbstractState* const state,
                                                 const double dt)
{
   const auto flightState = dynamic_cast<const FlightState*>(state);
   if (flightState == nullptr) return nullptr;

   snap = flightState->snapshot();
   if (!snap.valid) return nullptr;

   frameDt = dt;

   if (!treeBuilt) buildTree();
   if (!treeValid) return nullptr;

   currentDecision.reset();
   tree.tickRoot();
   if (!currentDecision.taken) return nullptr;

   // Acao PRE-REF'd (o Agent chama unref() depois de executar) -- contrato
   // do UBF.
   const auto action = new FlightAction();
   action->setCommand(currentDecision.command);
   action->setLabel(currentDecision.label);
   action->setVote(getVote());
   return action;
}

bool BtBehavior::setSlotTreeFile(const base::String* const msg)
{
   if (msg == nullptr || msg->getString() == nullptr) return false;
   treeFile_ = msg->getString();
   return true;
}

} // namespace xC_130
} // namespace models
} // namespace mixr
