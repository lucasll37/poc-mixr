#include "app/DashboardWiring.hpp"

#include "app/BackgroundPanel.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

//------------------------------------------------------------------------------
// A aba "Tempo Nao-Critico" (F4): o que roda na thread de tempo NAO critico --
// painel ESTATICO, sem lista (ver app/BackgroundPanel.hpp). 'w.displayedBackground'
// e alimentado pelo Renderer mais externo de app/DashboardLoop.cpp -- nao ha
// necessidade de tomar mutex nenhum aqui.
//------------------------------------------------------------------------------
namespace app {

ftxui::Component buildBackgroundTab(DashboardWiring& w)
{
   return ftxui::Renderer([&w]() -> ftxui::Element {
      return renderBackgroundPanel(w.displayedBackground) | ftxui::frame | ftxui::flex;
   });
}

} // namespace app
