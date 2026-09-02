#include "app/MetaObjectReport.hpp"


#include "xplugin/PluginRegistry.hpp"

#include "mixr/base/MetaObject.hpp"
#include "mixr/base/Pair.hpp"
#include "mixr/base/String.hpp"

#include <iostream>

namespace app {

namespace {

template <typename T>
void reportClass()
{
   const mixr::base::MetaObject* const meta{T::getMetaObject()};
   if (meta == nullptr) return;

   std::cout << "meta=" << meta->getFactoryName()
             << " count=" << meta->count
             << " mc=" << meta->mc
             << " tc=" << meta->tc
             << std::endl;
}

} // namespace

void printMetaObjectReport()
{
   // As classes do MODELO nao aparecem mais aqui -- elas viraram plugin, e
   // reportClass<T>() precisa do TIPO em tempo de compilacao. Elas reaparecem
   // no laco de pluginMetaObjects(), no fim desta funcao, no MESMO formato.

   // Sem shared/xmsg aqui (dashboard nao linka xmsg_dep -- nenhum cenario de
   // configs/ declara 'msgFeed:', o proprio TUI ja e o "feed"), entao nada de
   // mixr::xmsg::MsgFeed/MsgReport para reportar -- ao contrario das outras
   // pocs.

   // Termometro geral do parser/EDL.
   reportClass<mixr::base::Pair>();
   reportClass<mixr::base::String>();

   // As classes que vieram de PLUGIN.
   //
   // Nao podem entrar na lista acima, e isso e estrutural e nao esquecimento:
   // reportClass<T>() e template -- precisa do TIPO em tempo de compilacao --
   // e uma classe de plugin nao existe dentro do executavel. Pior: nao ha como
   // chegar ao MetaObject a partir de um Object*, porque getMetaObject() e
   // ESTATICA, nao virtual (macros.hpp:136).
   //
   // Por isso o descritor do plugin carrega os MetaObject* dele (campo 'metas'
   // de PluginDescV1). O formato da linha e o MESMO das de cima, entao
   // tests/memory/run_leak_test.py nao precisa saber que a classe veio de fora.
   //
   // Roda ANTES do SHUTDOWN e do station->unref(), com a imagem do plugin
   // garantidamente mapeada -- nunca damos dlclose (ver PluginRegistry.cpp).
   for (const mixr::base::MetaObject* const meta : mixr::xplugin::pluginMetaObjects()) {
      if (meta == nullptr) continue;
      std::cout << "meta=" << meta->getFactoryName()
                << " count=" << meta->count
                << " mc=" << meta->mc
                << " tc=" << meta->tc
                << std::endl;
   }
}

} // namespace app
