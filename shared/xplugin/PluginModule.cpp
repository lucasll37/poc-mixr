#include "xplugin/PluginModule.hpp"

#include "mixr/base/Identifier.hpp"
#include "mixr/base/List.hpp"
#include "mixr/base/Pair.hpp"
#include "mixr/base/PairStream.hpp"
#include "mixr/base/String.hpp"

namespace mixr {
namespace xplugin {

IMPLEMENT_SUBCLASS(PluginModule, "PluginModule")

// clang-format off
BEGIN_SLOTTABLE(PluginModule)
   "file",       // 1: arquivo .so
   "provides",   // 2: nomes de fabrica esperados (assercao)
END_SLOTTABLE(PluginModule)

BEGIN_SLOT_MAP(PluginModule)
   ON_SLOT(1, setSlotFile,     base::String)
   ON_SLOT(2, setSlotProvides, base::PairStream)
END_SLOT_MAP()

EMPTY_DELETEDATA(PluginModule)

PluginModule::PluginModule()
// clang-format on
{
   STANDARD_CONSTRUCTOR()
}

void PluginModule::copyData(const PluginModule& org, const bool)
{
   BaseClass::copyData(org);
   file_ = org.file_;
   provides_ = org.provides_;
}

bool PluginModule::setSlotFile(const base::String* const x)
{
   if (x == nullptr || x->getString() == nullptr) return false;
   file_ = x->getString();
   return !file_.empty();
}

bool PluginModule::setSlotProvides(const base::PairStream* const x)
{
   provides_.clear();
   if (x == nullptr) return false;

   // Mesma armadilha ja documentada em shared/xmsg (armadilha 1 da secao do
   // xmsg no CLAUDE.md): numa lista '{ a b c }' os itens sao ANONIMOS -- o
   // parser numera os slots ("1", "2", ...) e o nome real vai no VALOR, como
   // base::Identifier. Ja em '{ chave: valor }' o nome vai no slot. Ler o
   // OBJETO primeiro e cair para o slot cobre as duas formas, e cobre de
   // quebra a armadilha irma: valor sem aspas e Identifier, nao String.
   const base::List::Item* item{x->getFirstItem()};
   while (item != nullptr) {
      const auto pair = static_cast<const base::Pair*>(item->getValue());
      item = item->getNext();
      if (pair == nullptr) continue;

      const base::Object* const obj{pair->object()};
      if (const auto id = dynamic_cast<const base::Identifier*>(obj)) {
         if (id->getString() != nullptr) { provides_.emplace_back(id->getString()); continue; }
      }
      if (const auto str = dynamic_cast<const base::String*>(obj)) {
         if (str->getString() != nullptr) { provides_.emplace_back(str->getString()); continue; }
      }
      if (pair->slot() != nullptr) provides_.emplace_back(pair->slot()->getString());
   }
   return true;
}

} // namespace xplugin
} // namespace mixr
