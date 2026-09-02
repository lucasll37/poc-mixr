#include "app/MetaObjectSnapshot.hpp"

#include "xplugin/PluginRegistry.hpp"

#include "mixr/base/MetaObject.hpp"
#include "mixr/base/Pair.hpp"
#include "mixr/base/String.hpp"

#include <algorithm>

namespace app {

ClassStat updateClassStat(const ClassStat& previous, const std::string& factoryName,
                          const bool fromPlugin, const int count, const int mc, const long tc)
{
   ClassStat s;
   s.factoryName = factoryName;
   s.fromPlugin = fromPlugin;
   s.count = count;
   s.mc = mc;
   s.tc = tc;

   s.countHistory = previous.countHistory;
   s.countHistory.push_back(count);
   if (static_cast<int>(s.countHistory.size()) > kHistoryWindow) {
      s.countHistory.erase(s.countHistory.begin());
   }

   // Cresceu sustentado: o mais novo e maior que o mais velho da janela, e
   // 'count' nunca caiu entre duas amostras consecutivas dentro dela. So
   // vale a pena julgar com a janela cheia -- amostra de menos gera falso
   // positivo/negativo por ruido de partida.
   if (static_cast<int>(s.countHistory.size()) == kHistoryWindow) {
      const bool grew{s.countHistory.front() < s.countHistory.back()};
      bool everDropped{};
      for (std::size_t i = 1; i < s.countHistory.size(); i++) {
         if (s.countHistory[i] < s.countHistory[i - 1]) { everDropped = true; break; }
      }
      s.suspectedLeak = grew && !everDropped;
   }

   return s;
}

namespace {

const ClassStat* findPrevious(const std::vector<ClassStat>& previous, const std::string& name)
{
   for (const auto& s : previous) {
      if (s.factoryName == name) return &s;
   }
   return nullptr;
}

void appendStat(std::vector<ClassStat>& out, const std::vector<ClassStat>& previous,
                const mixr::base::MetaObject* const meta, const bool fromPlugin)
{
   if (meta == nullptr) return;
   const std::string name{meta->getFactoryName()};
   const ClassStat* const prev{findPrevious(previous, name)};
   static const ClassStat empty{};
   out.push_back(updateClassStat(prev != nullptr ? *prev : empty, name, fromPlugin,
                                 meta->count, meta->mc, meta->tc));
}

} // namespace

std::vector<ClassStat> sampleMetaObjects(const std::vector<ClassStat>& previous)
{
   std::vector<ClassStat> out;

   // Termometro do host -- mesmas duas classes que app/MetaObjectReport.cpp
   // ja usa como sinal geral do parser/EDL.
   appendStat(out, previous, mixr::base::Pair::getMetaObject(), false);
   appendStat(out, previous, mixr::base::String::getMetaObject(), false);

   // As classes que vieram de PLUGIN -- ja agnostico a qual modelo esta
   // carregado (ver o cabecalho deste arquivo).
   for (const mixr::base::MetaObject* const meta : mixr::xplugin::pluginMetaObjects()) {
      appendStat(out, previous, meta, true);
   }

   return out;
}

} // namespace app
