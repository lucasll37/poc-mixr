#include "app/DashboardWiring.hpp"

#include "app/LogPanel.hpp"

#include "xlog/Log.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/dom/elements.hpp>

#include <cstddef>

//------------------------------------------------------------------------------
// A aba "Log" (F5): as ultimas linhas de libs/xlog, do core E do plugin do
// modelo (uma copia so de libxlog.so no processo -- ver o cabecalho de
// app/LogPanel.hpp). Mesmo padrao de lista rolavel das abas Players/Memoria.
//
// 'w.doCycleLogFilter'/'w.doToggleLogFollow' sao chamadas tanto pelos
// botoes desta aba quanto pelo CatchEvent global ('f'/'a' com 'activeTab==4',
// ver app/DashboardLoop.cpp) -- por isso viram campos em vez de lambdas
// locais.
//------------------------------------------------------------------------------
namespace app {

namespace {
using namespace ftxui;
} // namespace

Component buildLogTab(DashboardWiring& w)
{
   MenuOption logMenuOpt;
   logMenuOpt.entries = &w.logLabels;
   logMenuOpt.selected = &w.selectedLogIndex;
   logMenuOpt.entries_option.transform = [&w](const EntryState& es) -> Element {
      if (es.index >= 0 && es.index < static_cast<int>(w.displayedLogs.size()))
         return renderLogRow(w.displayedLogs[static_cast<std::size_t>(es.index)], es.active);
      return text(es.label);
   };
   const Component logMenu{Menu(logMenuOpt)};

   w.doCycleLogFilter = [&w] {
      w.logMinLevel = nextLevelFilter(w.logMinLevel);
      w.logFollowTail = true;
   };
   w.doToggleLogFollow = [&w] { w.logFollowTail = !w.logFollowTail; };

   ButtonOption logFilterOpt;
   logFilterOpt.on_click = w.doCycleLogFilter;
   logFilterOpt.transform = [&w](const EntryState&) {
      return text(std::string(" [f] Nivel min: ") + mixr::xlog::levelName(w.logMinLevel) + " ")
         | bgcolor(Color::Blue) | bold;
   };
   const Component btnLogFilter{Button(logFilterOpt)};

   ButtonOption logFollowOpt;
   logFollowOpt.on_click = w.doToggleLogFollow;
   logFollowOpt.transform = [&w](const EntryState&) {
      return text(std::string(" [a] Acompanhar: ") + (w.logFollowTail ? "ON" : "OFF") + " ")
         | (w.logFollowTail ? (bgcolor(Color::Blue) | bold) : dim);
   };
   const Component btnLogFollow{Button(logFollowOpt)};

   const Component logButtons{Container::Horizontal({btnLogFilter, btnLogFollow})};
   const Component logBody{Container::Vertical({logMenu, logButtons})};
   return Renderer(logBody, [&w, logMenu, logButtons]() -> Element {
      return vbox({
         hbox({
            text("linhas: " + std::to_string(w.displayedLogs.size())) | dim,
            text("  (buffer de " + std::to_string(mixr::xlog::kMemoryCapacity)
                 + ", o mais antigo sai)") | dim,
            filler(),
            text("total emitido: " + std::to_string(w.lastLogSeq)) | dim,
         }),
         separator(),
         renderLogListHeader(),
         separator(),
         logMenu->Render() | vscroll_indicator | frame | flex,
         separator(),
         logButtons->Render(),
      });
   });
}

} // namespace app
