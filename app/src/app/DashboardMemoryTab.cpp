#include "app/DashboardWiring.hpp"

#include "app/MemoryPanel.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/dom/elements.hpp>

#include <cstddef>

//------------------------------------------------------------------------------
// A aba "Memoria" (F3): contadores de instancia AO VIVO, ver
// app/MetaObjectSnapshot.hpp para o criterio de "CRESCENDO"/"RACA". Nenhuma
// acao desta aba e' usada fora dela -- nao ha campo novo em DashboardWiring
// alem dos de estado ja compartilhado ('selectedClassIndex'/
// 'displayedClasses'/'classLabels', escritos pelo Renderer mais externo em
// DashboardLoop.cpp; a navegacao por seta e' tratada la' tambem, ver o
// comentario sobre 'Container::Vertical so encaminhar teclado ao filho
// focado' em app/DashboardMapTab.cpp).
//------------------------------------------------------------------------------
namespace app {

namespace {
using namespace ftxui;
} // namespace

Component buildMemoryTab(DashboardWiring& w)
{
   MenuOption classMenuOpt;
   classMenuOpt.entries = &w.classLabels;
   classMenuOpt.selected = &w.selectedClassIndex;
   classMenuOpt.entries_option.transform = [&w](const EntryState& es) -> Element {
      if (es.index >= 0 && es.index < static_cast<int>(w.displayedClasses.size()))
         return renderClassRow(w.displayedClasses[static_cast<std::size_t>(es.index)], es.active);
      return text(es.label);
   };
   const Component classMenu{Menu(classMenuOpt)};
   return Renderer(classMenu, [&w, classMenu]() -> Element {
      return vbox({
         text("classes observadas: " + std::to_string(w.displayedClasses.size())) | dim,
         classMenu->Render() | vscroll_indicator | frame | flex,
      });
   });
}

} // namespace app
