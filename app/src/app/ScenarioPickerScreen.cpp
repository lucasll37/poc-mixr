#include "app/ScenarioPickerScreen.hpp"

#include "app/ScenarioCatalog.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <vector>

namespace app {

namespace {
using namespace ftxui;

// Tela FIXA -- largura/altura EQUAL, nao GREATER_THAN (ver o comentario
// grande no Renderer, mais abaixo). 'kPickerDescLines' cobre a descricao
// mais comprida do catalogo (~80 caracteres) em 2 linhas com folga na
// largura de 'kPickerWidth'; se um cenario futuro ganhar uma descricao bem
// mais longa, e so alongar 'kPickerDescLines' -- nao existe caminho pra isso
// acontecer sozinho e voltar a "pular" tela.
const int kPickerWidth{76};
const int kPickerDescLines{2};
const int kPickerHeight{12};
}

std::string runScenarioPicker()
{
   const auto& catalog{scenarioCatalog()};

   std::vector<std::string> labels;
   labels.reserve(catalog.size());
   for (const auto& entry : catalog) labels.push_back(entry.label);

   int selected{};
   bool confirmed{};
   bool cancelled{};

   auto screen = ScreenInteractive::Fullscreen();

   auto menuOption = MenuOption::Vertical();
   menuOption.entries = &labels;
   menuOption.selected = &selected;
   menuOption.on_enter = [&] { confirmed = true; screen.Exit(); };

   const Component menu{Menu(menuOption)};

   // Largura e altura FIXAS ('EQUAL', nao 'GREATER_THAN') -- a descricao de
   // cada cenario tem um comprimento diferente (ver ScenarioCatalog.cpp), e
   // com 'GREATER_THAN' o box crescia/encolhia (e recentralizava, por causa
   // do 'center') conforme o numero de linhas que 'paragraphAlignLeft'
   // precisava pra encaixar cada descricao -- a tela "pulava" so de navegar
   // entre as opcoes. Reservar 2 linhas fixas pra descricao (cabe as tres
   // deste catalogo, mesmo a mais comprida, no 'kPickerWidth' escolhido)
   // resolve pra qualquer selecao, sem depender do conteudo.
   const auto root = Renderer(menu, [&] {
      Elements items;
      items.push_back(text("selecione um cenario") | bold | color(Color::CyanLight));
      items.push_back(separator());
      items.push_back(menu->Render() | flex);
      items.push_back(separator());
      const std::string& desc{(selected >= 0 && selected < static_cast<int>(catalog.size()))
                              ? catalog[static_cast<std::size_t>(selected)].description
                              : std::string{}};
      items.push_back(paragraphAlignLeft(desc) | color(Color::GrayLight)
                      | size(HEIGHT, EQUAL, kPickerDescLines));
      items.push_back(separator());
      items.push_back(text("[up/down] navegar   [enter] carregar   [q] sair") | dim);

      return vbox(std::move(items)) | border | size(WIDTH, EQUAL, kPickerWidth)
             | size(HEIGHT, EQUAL, kPickerHeight) | center;
   });

   const Component withQuit = CatchEvent(root, [&](const Event& event) {
      if (event == Event::Character('q') || event == Event::Character('Q') ||
          event == Event::Escape) {
         cancelled = true;
         screen.Exit();
         return true;
      }
      return false;
   });

   screen.Loop(withQuit);

   if (cancelled || !confirmed) return {};
   if (selected < 0 || selected >= static_cast<int>(catalog.size())) return {};
   return catalog[static_cast<std::size_t>(selected)].key;
}

} // namespace app
