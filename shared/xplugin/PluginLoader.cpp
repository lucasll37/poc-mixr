#include "xplugin/PluginLoader.hpp"

#include "xplugin/PluginModule.hpp"
#include "xplugin/PluginRegistry.hpp"

#include "mixr/base/Identifier.hpp"
#include "mixr/base/List.hpp"
#include "mixr/base/Pair.hpp"
#include "mixr/base/PairStream.hpp"
#include "mixr/base/String.hpp"

#include <cstdlib>
#include <iostream>

namespace mixr {
namespace xplugin {

IMPLEMENT_SUBCLASS(PluginLoader, "PluginLoader")

BEGIN_SLOTTABLE(PluginLoader)
   "searchPaths",   // 1: onde procurar as .so, na ordem
   "modules",       // 2: o que carregar
END_SLOTTABLE(PluginLoader)

BEGIN_SLOT_MAP(PluginLoader)
   ON_SLOT(1, setSlotSearchPaths, base::PairStream)
   ON_SLOT(2, setSlotModules,     base::PairStream)
END_SLOT_MAP()

PluginLoader::PluginLoader()
{
   STANDARD_CONSTRUCTOR()
}

void PluginLoader::copyData(const PluginLoader& org, const bool cc)
{
   BaseClass::copyData(org);

   if (cc) modules_ = nullptr;

   searchPaths_ = org.searchPaths_;

   if (modules_ != nullptr) { modules_->unref(); modules_ = nullptr; }
   if (org.modules_ != nullptr) { modules_ = org.modules_; modules_->ref(); }
}

void PluginLoader::deleteData()
{
   if (modules_ != nullptr) { modules_->unref(); modules_ = nullptr; }
}

bool PluginLoader::setSlotSearchPaths(const base::PairStream* const x)
{
   searchPaths_.clear();
   if (x == nullptr) return false;

   // Mesma leitura de lista de PluginModule::setSlotProvides -- ver o
   // comentario la para a armadilha dos itens anonimos do EDL.
   const base::List::Item* item{x->getFirstItem()};
   while (item != nullptr) {
      const auto pair = static_cast<const base::Pair*>(item->getValue());
      item = item->getNext();
      if (pair == nullptr) continue;

      const base::Object* const obj{pair->object()};
      if (const auto str = dynamic_cast<const base::String*>(obj)) {
         if (str->getString() != nullptr) { searchPaths_.emplace_back(str->getString()); continue; }
      }
      if (const auto id = dynamic_cast<const base::Identifier*>(obj)) {
         if (id->getString() != nullptr) { searchPaths_.emplace_back(id->getString()); continue; }
      }
   }
   return true;
}

bool PluginLoader::setSlotModules(base::PairStream* const x)
{
   if (modules_ != nullptr) { modules_->unref(); modules_ = nullptr; }
   if (x == nullptr) return false;
   modules_ = x;
   modules_->ref();
   return true;
}

bool PluginLoader::isValid() const
{
   if (!BaseClass::isValid()) return false;

   if (modules_ == nullptr) {
      // Bloco vazio e legitimo (um cenario que declara a secao mas nao carrega
      // nada ainda). Nao e erro; so nao ha o que fazer.
      std::cerr << "[plugin] ( PluginLoader ) sem 'modules:' -- nada a carregar." << std::endl;
      return true;
   }

   const base::List::Item* item{modules_->getFirstItem()};
   while (item != nullptr) {
      const auto pair = static_cast<const base::Pair*>(item->getValue());
      item = item->getNext();
      if (pair == nullptr) continue;

      const auto mod = dynamic_cast<const PluginModule*>(pair->object());
      if (mod == nullptr) {
         const base::Object* const obj{pair->object()};
         std::cerr << "[plugin] 'modules:' aceita so ( PluginModule ); veio '"
                   << (obj != nullptr ? obj->getFactoryName() : "(nulo)") << "'." << std::endl;
         std::exit(EXIT_FAILURE);
      }
      if (mod->file().empty()) {
         std::cerr << "[plugin] ( PluginModule ) sem 'file:' -- nao da para saber o que abrir."
                   << std::endl;
         std::exit(EXIT_FAILURE);
      }

      // Fatal em qualquer falha -- ver o cabecalho de PluginLoader.hpp.
      loadModule(mod->file(), searchPaths_, mod->provides());
   }
   return true;
}

} // namespace xplugin
} // namespace mixr
