#include "ubf/ExampleBehavior.hpp"

#include "ubf/ExampleAction.hpp"
#include "ubf/ExampleState.hpp"

#include "bt/bt_factory.hpp"

#include "xlog/Log.hpp"

#include "mixr/base/String.hpp"
#include "mixr/base/units/Distances.hpp"

#include <mutex>

namespace mixr {
namespace models {
namespace xtemplate {

namespace {

// BT::BehaviorTreeFactory NAO e' thread-safe na CONSTRUCAO da arvore, e um
// cenario com N entidades constroi N arvores -- potencialmente em paralelo,
// se o agente do UBF decidir na fase 3 do frame de tempo critico (que e' o
// caso de ( FlightAgentTC ), o agente que os cenarios deste repositorio
// usam). Serializar so' a construcao e' barato: acontece uma vez por
// entidade, nunca por tick. Mesmo padrao de models/players/A-4.
std::mutex g_treeBuildMutex;

} // namespace

IMPLEMENT_SUBCLASS(ExampleBehavior, "ExampleBehavior")

// clang-format off
BEGIN_SLOTTABLE(ExampleBehavior)
   "treeFile",   // 1
   "onValue",    // 2
   "offValue",   // 3
END_SLOTTABLE(ExampleBehavior)

BEGIN_SLOT_MAP(ExampleBehavior)
   ON_SLOT(1, setSlotTreeFile, base::String)
   ON_SLOT(2, setSlotOnValue,  base::Distance)
   ON_SLOT(3, setSlotOffValue, base::Distance)
END_SLOT_MAP()

EMPTY_DELETEDATA(ExampleBehavior)

ExampleBehavior::ExampleBehavior()
// clang-format on
{
   STANDARD_CONSTRUCTOR()
}

void ExampleBehavior::copyData(const ExampleBehavior& org, const bool)
{
   BaseClass::copyData(org);
   rule_ = org.rule_;
   engaged_ = org.engaged_;
   value_ = org.value_;
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

void ExampleBehavior::reset()
{
   BaseClass::reset();
   engaged_ = false;
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
void ExampleBehavior::buildTree()
{
   treeBuilt_ = true;

   if (treeFile_.empty()) {
      LOG(WARNING) << "[ExampleBehavior] slot 'treeFile' vazio -- arvore nao construida";
      return;
   }

   bt::NodeContext context;
   context.behavior = this;

   std::lock_guard<std::mutex> lock(g_treeBuildMutex);

   // Factory NOVA a cada tentativa: registerBuilder() lanca
   // "ID already registered" pra um ID ja presente, entao um segundo
   // buildTree() sobre a MESMA factory (possivel depois de reset()) morreria
   // no primeiro no -- fora do try/catch abaixo, que so' cobre a leitura do
   // arquivo. Mesmo achado ja registrado em models/players/A-4.
   btFactory_ = BT::BehaviorTreeFactory();
   bt::registerNodes(btFactory_, context);

   try {
      tree_ = btFactory_.createTreeFromFile(treeFile_, BT::Blackboard::create());
      treeValid_ = true;
   } catch (const std::exception& ex) {
      LOG(ERROR) << "[ExampleBehavior] falha ao carregar a arvore '" << treeFile_
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
base::ubf::AbstractAction* ExampleBehavior::genAction(const base::ubf::AbstractState* const state,
                                                      const double)
{
   const auto s = dynamic_cast<const ExampleState*>(state);
   if (s == nullptr) return nullptr;

   value_ = s->getValue();

   if (!treeBuilt_) buildTree();

   decision_.reset();

   if (treeValid_) {
      tree_.tickRoot();
   } else {
      // DEGRADACAO, nao falha: sem arvore carregada o modelo continua
      // decidindo pela MESMA regra pura, direto. E' o que mantem a entidade
      // voando (e o cenario rodando) quando o .xml some do lugar ou nao
      // parseia -- o erro ja foi para o log, uma vez, em buildTree().
      engaged_ = rule_.next(value_, engaged_);
      decision_.take(engaged_ ? "ENGAGED" : "IDLE");
   }

   const auto action = new ExampleAction(decision_.label);   // pre-ref'd: o Agent da unref()
   action->setVote(getVote());
   return action;
}

bool ExampleBehavior::setSlotTreeFile(const base::String* const msg)
{
   // getString(), nao c_str(): base::String guarda um 'char*' PRIVADO e
   // expoe so' este getter (mais um operator const char*). Pode voltar
   // nullptr num objeto default-construido.
   if (msg == nullptr || msg->getString() == nullptr) return false;
   treeFile_ = msg->getString();
   return true;
}

bool ExampleBehavior::setSlotOnValue(const base::Distance* const msg)
{
   if (msg == nullptr) return false;
   rule_.onValue = base::Meters::convertStatic(*msg);
   return true;
}

bool ExampleBehavior::setSlotOffValue(const base::Distance* const msg)
{
   if (msg == nullptr) return false;
   rule_.offValue = base::Meters::convertStatic(*msg);
   return true;
}

} // namespace xtemplate
} // namespace models
} // namespace mixr
