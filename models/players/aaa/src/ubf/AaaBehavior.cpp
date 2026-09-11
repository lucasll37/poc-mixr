#include "ubf/AaaBehavior.hpp"

#include "ubf/AaaAction.hpp"
#include "ubf/AaaState.hpp"

#include "bt/bt_factory.hpp"

#include "xlog/Log.hpp"

#include "mixr/base/String.hpp"

#include <mutex>

namespace mixr {
namespace models {
namespace xaaa {

namespace {

// Mesma protecao de models/players/A-4 e do template que originou este
// scaffold: BT::BehaviorTreeFactory NAO e' thread-safe na CONSTRUCAO da
// arvore, e um cenario com N antiaereas constroi N arvores -- possivelmente
// em paralelo, se ( UbfAgent ) decidir a partir de threads distintas do
// pool de fundo. Serializar so' a construcao e' barato: acontece uma vez
// por entidade, nunca por tick.
std::mutex g_treeBuildMutex;

} // namespace

IMPLEMENT_SUBCLASS(AaaBehavior, "AaaBehavior")

// clang-format off
BEGIN_SLOTTABLE(AaaBehavior)
   "treeFile",   // 1
END_SLOTTABLE(AaaBehavior)

BEGIN_SLOT_MAP(AaaBehavior)
   ON_SLOT(1, setSlotTreeFile, base::String)
END_SLOT_MAP()

EMPTY_DELETEDATA(AaaBehavior)

AaaBehavior::AaaBehavior()
// clang-format on
{
   STANDARD_CONSTRUCTOR()
}

void AaaBehavior::copyData(const AaaBehavior& org, const bool)
{
   BaseClass::copyData(org);
   hasTarget_ = org.hasTarget_;
   targetName_ = org.targetName_;
   targetRangeM_ = org.targetRangeM_;
   weaponReady_ = org.weaponReady_;
   dome_ = org.dome_;
   decision_ = org.decision_;
   treeFile_ = org.treeFile_;

   // A arvore NAO e' copiada: BT::Tree guarda os nos construidos, e cada no
   // carrega um NodeContext apontando para o comportamento que o hospeda --
   // copiar traria ponteiros para o OUTRO objeto. Reconstruir e' o unico
   // caminho correto, e e' o que 'treeBuilt_ = false' aqui provoca no
   // proximo genAction().
   treeBuilt_ = false;
   treeValid_ = false;
   tree_ = BT::Tree();
}

void AaaBehavior::reset()
{
   BaseClass::reset();
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
void AaaBehavior::buildTree()
{
   treeBuilt_ = true;

   if (treeFile_.empty()) {
      LOG(WARNING) << "[AaaBehavior] slot 'treeFile' vazio -- arvore nao construida";
      return;
   }

   bt::NodeContext context;
   context.behavior = this;

   std::lock_guard<std::mutex> lock(g_treeBuildMutex);

   // Factory NOVA a cada tentativa: registerBuilder() lanca
   // "ID already registered" pra um ID ja presente, entao um segundo
   // buildTree() sobre a MESMA factory (possivel depois de reset()) morreria
   // no primeiro no. Mesmo achado ja registrado em models/players/A-4.
   btFactory_ = BT::BehaviorTreeFactory();
   bt::registerNodes(btFactory_, context);

   try {
      tree_ = btFactory_.createTreeFromFile(treeFile_, BT::Blackboard::create());
      treeValid_ = true;
   } catch (const std::exception& ex) {
      LOG(ERROR) << "[AaaBehavior] falha ao carregar a arvore '" << treeFile_
                 << "': " << ex.what();
      treeValid_ = false;
   }
}

//------------------------------------------------------------------------------
// genAction() -- chamado uma vez por ciclo de decisao pelo Agent do UBF.
//
// SEMPRE devolve uma acao (nunca nullptr): mesmo sem alvo no domo, a arvore
// ainda decide "observar" (bt::WatchAction) -- e' o que garante que o
// xboard e' escrito TODO tick (ver CONTRATO.md secao 3, a obrigacao que
// falha em silencio). Sem arvore carregada (erro de config), a degradacao
// e' a MESMA coisa: observar, nunca disparar por conta propria -- um
// sistema de armas nao deve ter um "modo de disparo automatico por default"
// quando a configuracao esta incompleta.
//------------------------------------------------------------------------------
base::ubf::AbstractAction* AaaBehavior::genAction(const base::ubf::AbstractState* const state,
                                                  const double)
{
   const auto s = dynamic_cast<const AaaState*>(state);
   if (s == nullptr) return nullptr;

   hasTarget_ = s->hasTarget();
   targetName_ = s->targetName();
   targetRangeM_ = s->targetRangeM();
   weaponReady_ = s->weaponReady();
   dome_ = s->dome();

   if (!treeBuilt_) buildTree();

   decision_.reset();

   if (treeValid_) {
      tree_.tickRoot();
   } else {
      decision_.watch();
   }

   const auto action = new AaaAction(decision_.label, decision_.fireRequested, decision_.targetName);
   action->setVote(getVote());
   return action;   // pre-ref'd: o Agent da' unref()
}

bool AaaBehavior::setSlotTreeFile(const base::String* const msg)
{
   // getString(), nao c_str(): base::String guarda um 'char*' PRIVADO e
   // expoe so' este getter (mais um operator const char*). Pode voltar
   // nullptr num objeto default-construido.
   if (msg == nullptr || msg->getString() == nullptr) return false;
   treeFile_ = msg->getString();
   return true;
}

} // namespace xaaa
} // namespace models
} // namespace mixr
