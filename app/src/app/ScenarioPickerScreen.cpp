#include "app/ScenarioPickerScreen.hpp"

#include "app/ScenarioCatalog.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>

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

   // Caixa de tela da lista apos o ultimo desenho (ftxui::reflect) -- e o
   // que permite saber se um clique caiu DENTRO da lista antes de trata-lo
   // como escolha. Mesmo padrao ja usado no gate de mouse do mapa (ver
   // app/DashboardLoop.cpp).
   Box menuBox{};

   auto screen = ScreenInteractive::Fullscreen();

   const auto confirm = [&] { confirmed = true; screen.Exit(); };

   auto menuOption = MenuOption::Vertical();
   menuOption.entries = &labels;
   menuOption.selected = &selected;
   menuOption.on_enter = confirm;

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
      items.push_back(menu->Render() | reflect(menuBox) | flex);
      items.push_back(separator());
      const std::string& desc{(selected >= 0 && selected < static_cast<int>(catalog.size()))
                              ? catalog[static_cast<std::size_t>(selected)].description
                              : std::string{}};
      items.push_back(paragraphAlignLeft(desc) | color(Color::GrayLight)
                      | size(HEIGHT, EQUAL, kPickerDescLines));
      items.push_back(separator());
      items.push_back(text("[up/down] navegar   [enter] ou [clique] carregar   [q] sair") | dim);

      return vbox(std::move(items)) | border | size(WIDTH, EQUAL, kPickerWidth)
             | size(HEIGHT, EQUAL, kPickerHeight) | center;
   });

   // CLICAR numa opcao carrega o cenario, igual ao Enter (pedido explicito).
   //
   // O ftxui::Menu, sozinho, NAO faz isso: lendo o fonte da lib
   // (src/ftxui/component/menu.cpp, MenuImpl::OnMouseEvent) um clique
   // esquerdo com motion == Mouse::Pressed dentro da caixa de uma entrada
   // so faz 'selected() = i' e chamar on_change -- on_enter NAO e chamado
   // por mouse em lugar nenhum. Pior: o ajuste esta dentro de um
   // 'if (selected() != i)', entao clicar na entrada JA selecionada nao
   // produz evento nenhum.
   //
   // Por isso a confirmacao e feita aqui, no RELEASE:
   //   - o Pressed passa adiante (return false) para o proprio Menu fazer
   //     o que ja faz de melhor -- mover 'selected' para a entrada clicada;
   //   - o Released, que o Menu ignora, vira a confirmacao.
   // Nesta ordem o clique unico sempre carrega a opcao sob o cursor,
   // inclusive quando ela ja era a selecionada.
   //
   // NAO da pra usar on_change como confirmacao: ele tambem dispara na
   // navegacao por seta, e cada seta carregaria um cenario.
   const Component withMouse = CatchEvent(root, [&](Event event) {
      if (!event.is_mouse()) return false;
      if (event.mouse().button != Mouse::Left) return false;
      if (!menuBox.Contain(event.mouse().x, event.mouse().y)) return false;
      if (event.mouse().motion != Mouse::Released) return false;   // Pressed: deixa o Menu selecionar

      confirm();
      return true;
   });

   const Component withQuit = CatchEvent(withMouse, [&](const Event& event) {
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
