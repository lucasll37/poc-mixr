#include "app/ScenarioPickerScreen.hpp"

#include "app/BannerImage.hpp"
#include "app/PickerGeometry.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>
#include <ftxui/screen/terminal.hpp>

#include <vector>

namespace app {

namespace {
using namespace ftxui;

Color toFtxuiColor(const BannerRgb& c) { return Color::RGB(c.r, c.g, c.b); }

// O glifo de meio-bloco: cor de primeiro plano = subpixel de CIMA, cor de
// fundo = subpixel de BAIXO -- ver o comentario de BannerCell no header.
// Construido celula a celula (nao Canvas) porque cada celula precisa de uma
// cor RGB INDEPENDENTE por metade -- o Canvas de blocos do FTXUI nao da'
// essa granularidade de cor.
Element renderBannerBackground(const std::vector<BannerCell>& cells, const int cellsWide, const int cellsTall)
{
   if (cells.empty() || cellsWide <= 0 || cellsTall <= 0) return text("");

   Elements rows;
   rows.reserve(static_cast<std::size_t>(cellsTall));
   for (int y = 0; y < cellsTall; ++y) {
      Elements row;
      row.reserve(static_cast<std::size_t>(cellsWide));
      for (int x = 0; x < cellsWide; ++x) {
         const BannerCell& cell{cells[static_cast<std::size_t>(y) * static_cast<std::size_t>(cellsWide)
                                       + static_cast<std::size_t>(x)]};
         row.push_back(text("▀") | color(toFtxuiColor(cell.top)) | bgcolor(toFtxuiColor(cell.bottom)));
      }
      rows.push_back(hbox(std::move(row)));
   }
   return vbox(std::move(rows)) | size(WIDTH, EQUAL, cellsWide) | size(HEIGHT, EQUAL, cellsTall);
}

}

std::string runPickerScreen(const std::vector<PickerItem>& items, const std::string& title)
{
   std::vector<std::string> labels;
   labels.reserve(items.size());
   for (const auto& item : items) labels.push_back(item.label);

   int selected{};
   bool confirmed{};
   bool cancelled{};

   // Calculado UMA VEZ, na construcao da tela (ver o "porque" em
   // app/PickerGeometry.hpp) -- 'items' nao muda durante a sessao do
   // picker, entao fixar a altura a partir do TAMANHO da lista nao
   // reintroduz o problema de "tela pulando" que motivou height:EQUAL em
   // vez de GREATER_THAN (aquele bug era por FRAME, a cada troca de
   // selecao/descricao -- este numero so muda entre uma chamada e a
   // proxima).
   const pickergeometry::Geometry geometry{
      pickergeometry::computeGeometry(static_cast<int>(items.size()))};
   const int menuLines{geometry.menuLines};
   const int pickerHeight{geometry.totalHeight};

   // Caixa de tela da lista apos o ultimo desenho (ftxui::reflect) -- e o
   // que permite saber se um clique caiu DENTRO da lista antes de trata-lo
   // como escolha. Mesmo padrao ja usado no gate de mouse do mapa (ver
   // app/DashboardLoop.cpp).
   Box menuBox{};

   // Decodificada UMA VEZ, aqui -- nunca a cada redesenho (stb_image e'
   // barato pra uma imagem deste tamanho, mas nao precisa rodar de novo a
   // cada tecla/clique). width==0 (arquivo ausente ou decodificacao
   // falhou) degrada em silencio: 'renderBannerBackground' com 'cells'
   // vazio devolve um elemento neutro, e a tela sai identica a antes desta
   // funcionalidade existir.
   const BannerPixels bannerImage{loadBannerImage()};

   // Reamostrada so' quando o TAMANHO DO TERMINAL muda -- o resto do laco
   // (setas, digitacao) nao teria motivo pra recalcular ~dezenas de
   // milhares de subpixels a cada evento. Mesmo raciocinio ja usado por
   // 'fitMapCanvasToBox()' (app/MapPanel.cpp): so refaz o trabalho quando a
   // caixa de fato mudou.
   int cachedTermW{-1};
   int cachedTermH{-1};
   std::vector<BannerCell> cachedBannerCells;

   auto screen = ScreenInteractive::Fullscreen();

   const auto confirm = [&] { confirmed = true; screen.Exit(); };

   auto menuOption = MenuOption::Vertical();
   menuOption.entries = &labels;
   menuOption.selected = &selected;
   menuOption.on_enter = confirm;

   const Component menu{Menu(menuOption)};

   // Largura e altura FIXAS ('EQUAL', nao 'GREATER_THAN') -- a descricao de
   // cada item pode ter um comprimento diferente, e com 'GREATER_THAN' o box
   // crescia/encolhia (e recentralizava, por causa do 'center') conforme o
   // numero de linhas que 'paragraphAlignLeft' precisava pra encaixar cada
   // descricao -- a tela "pulava" so de navegar entre as opcoes. Reservar 2
   // linhas fixas pra descricao (cabe a mais comprida vista ate hoje, no
   // 'pickergeometry::kWidth' escolhido) resolve pra qualquer selecao, sem
   // depender do conteudo.
   //
   // O MENU em si NAO e mais 'flex' (que so herdava o que sobrasse da altura
   // TOTAL fixa, e travava em pouquissimas linhas): agora tem sua propria
   // altura fixa ('menuLines', calculada acima a partir da CONTAGEM de
   // itens) e o mesmo idiom 'vscroll_indicator | frame' que
   // Players/Memoria/Log ja usam pra lista rolavel (DashboardLoop.cpp) --
   // com mais itens do que 'menuLines' cabe, a barra de rolagem aparece e
   // as setas continuam movendo 'selected' para fora da janela visivel, so
   // que agora ela ACOMPANHA.
   const auto root = Renderer(menu, [&] {
      Elements elems;
      elems.push_back(text(title) | bold | color(Color::CyanLight));
      elems.push_back(separator());
      elems.push_back(menu->Render() | reflect(menuBox) | vscroll_indicator | frame
                      | size(HEIGHT, EQUAL, menuLines));
      elems.push_back(separator());
      const std::string& desc{(selected >= 0 && selected < static_cast<int>(items.size()))
                              ? items[static_cast<std::size_t>(selected)].description
                              : std::string{}};
      elems.push_back(paragraphAlignLeft(desc) | color(Color::GrayLight)
                      | size(HEIGHT, EQUAL, pickergeometry::kDescLines));
      elems.push_back(separator());
      elems.push_back(text("[up/down] navegar   [enter] ou [clique] carregar   [q] sair") | dim);

      const Element dialog = vbox(std::move(elems)) | border | size(WIDTH, EQUAL, pickergeometry::kWidth)
                             | size(HEIGHT, EQUAL, pickerHeight);

      if (bannerImage.width == 0) return dialog | center;

      // O terminal pode ser redimensionado a qualquer momento (a 'root' e'
      // reconstruida a cada redesenho, nao so' na abertura da tela) -- se o
      // tamanho mudou desde a ultima vez, reamostra; senao reusa a grade ja
      // calculada. 'clear_under' e' o que torna a caixa do dialogo OPACA
      // por cima do fundo (mesmo padrao ja usado pelo dialogo de
      // confirmacao de app/DashboardLoop.cpp) -- sem ele, o fundo
      // "vazaria" pelas celulas nao explicitamente pintadas dentro da
      // caixa.
      const int termW{Terminal::Size().dimx};
      const int termH{Terminal::Size().dimy};
      if (termW != cachedTermW || termH != cachedTermH) {
         cachedBannerCells = resampleBannerForTerminal(bannerImage, termW, termH);
         cachedTermW = termW;
         cachedTermH = termH;
      }

      return dbox({
         renderBannerBackground(cachedBannerCells, termW, termH),
         dialog | clear_under | center,
      });
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
   if (selected < 0 || selected >= static_cast<int>(items.size())) return {};
   return items[static_cast<std::size_t>(selected)].key;
}

} // namespace app
