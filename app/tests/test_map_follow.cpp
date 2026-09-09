#include "app/MapPanel.hpp"
#include "app/MapGeometry.hpp"
#include "app/FleetPanel.hpp"

#include <gtest/gtest.h>

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>
#include <ftxui/screen/screen.hpp>

#include <limits>
#include <string>
#include <vector>

// O "seguir a entidade selecionada" da aba Mapa (app::applyMapFollow /
// MapViewState::followSelected): enquanto ligado, o pan e recolocado sobre a
// entidade a cada redesenho, entao ela fica SEMPRE no centro do canvas -- nas
// DUAS perspectivas, e sem tirar o zoom do usuario.
//
// O que este arquivo trava, e por que cada coisa:
//
//  1. A propriedade que o usuario pediu, como IGUALDADE EXATA de pixel
//     (px == canvasWidthPx/2, py == canvasHeightPx/2), varrida sobre
//     perspectiva x rotacao x zoom x tamanho de canvas. Ela ja era verdadeira
//     por construcao antes desta feature, mas NENHUM teste a afirmava -- o
//     mais proximo, MapGeometry.OnCanvasNoCentroDoPan, so checa 'onCanvas'
//     com pan default e um unico yaw.
//  2. Que zoom e rotacao NAO desligam o seguir (o pedido e explicito: "o zoom
//     ainda e a cargo do usuario") e nao tiram a entidade do centro.
//  3. Que pan MANUAL desliga -- o invariante que evita a vista parecer
//     travada (os dois disputam o mesmo pan). Fica dentro de panMap() de
//     proposito: e o unico ponto por onde arrasto e setas passam.
//  4. As bordas que o chamador nao deve precisar tratar: follow desligado,
//     lista vazia, indice fora de faixa.
//  5. Que o cabecalho desenhado no canvas de fato mostra "seguindo=<nome>" --
//     renderizado num ftxui::Screen de verdade, nao inspecionado por leitura.

using namespace app;
using namespace app::mapgeometry;
using namespace ftxui;

namespace {

// Mesmo estilo de app/tests/test_map_canvas_fit.cpp.
std::vector<EntityState> makeEntities()
{
   std::vector<EntityState> v;
   for (int i = 0; i < 4; i++) {
      EntityState e;
      e.id = i + 1;
      e.name = "falcon" + std::to_string(i + 1);
      e.northM = 500.0 * i;
      e.eastM = -300.0 * i;
      e.altitudeM = 1500.0 + 50.0 * i;
      e.headingDeg = 45.0 * i;
      v.push_back(e);
   }
   return v;
}

bool centradoExatamente(const MapViewState& view, const EntityState& e)
{
   const Projected p{project(e.northM, e.eastM, e.altitudeM, view)};
   return p.px == view.canvasWidthPx / 2 && p.py == view.canvasHeightPx / 2;
}

} // namespace

// ---------------------------------------------------------------------------
// 1. A propriedade pedida, nas duas perspectivas
// ---------------------------------------------------------------------------

TEST(MapFollow, EntidadeSeguidaFicaNoCentroExatoNasDuasPerspectivas)
{
   const auto entities{makeEntities()};
   const double yaws[]{0.0, 15.0, 45.0, 90.0, 180.0, 227.0, 315.0, 359.0};
   const double scales[]{2.0, 50.0, 1234.5, 200000.0};
   const int canvasSizes[][2]{{240, 120}, {160, 44}, {41, 27}, {2, 2}};

   for (const Perspective persp : {Perspective::TopDown, Perspective::Lateral}) {
      for (const double yaw : yaws) {
         for (const double scale : scales) {
            for (const auto& sz : canvasSizes) {
               for (int idx = 0; idx < static_cast<int>(entities.size()); idx++) {
                  MapViewState view;
                  view.followSelected = true;
                  view.perspective = persp;
                  view.viewYawDeg = yaw;
                  view.metersPerCell = scale;
                  view.canvasWidthPx = sz[0];
                  view.canvasHeightPx = sz[1];
                  // Pan longe da entidade de proposito -- o follow tem de
                  // trazer a vista ate ela, nao so "ja estava perto".
                  view.panNorthM = -98765.0;
                  view.panEastM = 43210.0;
                  view.panAltM = -7777.0;

                  ASSERT_TRUE(applyMapFollow(view, entities, idx));
                  EXPECT_TRUE(centradoExatamente(view, entities[static_cast<std::size_t>(idx)]))
                     << "persp=" << (persp == Perspective::TopDown ? "cima" : "lado")
                     << " yaw=" << yaw << " mpc=" << scale
                     << " canvas=" << sz[0] << "x" << sz[1] << " idx=" << idx;
               }
            }
         }
      }
   }
}

TEST(MapFollow, LateralCentralizaTambemNaALTITUDE)
{
   // A metade que so a perspectiva Lateral exercita: no TopDown 'panAltM'
   // nao participa da projecao, entao um follow que esquecesse de escrever
   // esse campo passaria despercebido de cima e falharia de lado.
   const auto entities{makeEntities()};
   MapViewState view;
   view.followSelected = true;
   view.perspective = Perspective::Lateral;
   view.panAltM = 0.0;

   ASSERT_TRUE(applyMapFollow(view, entities, 2));
   EXPECT_DOUBLE_EQ(view.panAltM, entities[2].altitudeM);

   const Projected p{project(entities[2].northM, entities[2].eastM, entities[2].altitudeM, view)};
   EXPECT_EQ(p.py, view.canvasHeightPx / 2);
}

TEST(MapFollow, SegueAEntidadeQUEEMOVE)
{
   // O caso real: a aeronave voa, e a cada quadro a mesma chamada tem de
   // trazer a vista junto (idempotente e sem acumular nada).
   std::vector<EntityState> entities{makeEntities()};
   MapViewState view;
   view.followSelected = true;

   for (int frame = 0; frame < 50; frame++) {
      entities[0].northM += 82.0;    // ~82 m/s, o cruzeiro do c310
      entities[0].eastM -= 17.0;
      entities[0].altitudeM += 3.0;
      ASSERT_TRUE(applyMapFollow(view, entities, 0));
      EXPECT_TRUE(centradoExatamente(view, entities[0])) << "frame=" << frame;
   }
}

TEST(MapFollow, ChamarVariasVezesNoMESMOQuadroNaoMudaNada)
{
   const auto entities{makeEntities()};
   MapViewState view;
   view.followSelected = true;

   applyMapFollow(view, entities, 1);
   const double n{view.panNorthM}, e{view.panEastM}, a{view.panAltM};
   for (int i = 0; i < 5; i++) applyMapFollow(view, entities, 1);
   EXPECT_DOUBLE_EQ(view.panNorthM, n);
   EXPECT_DOUBLE_EQ(view.panEastM, e);
   EXPECT_DOUBLE_EQ(view.panAltM, a);
}

// ---------------------------------------------------------------------------
// 2. Zoom e rotacao continuam do usuario
// ---------------------------------------------------------------------------

TEST(MapFollow, ZoomNaoDesligaOSeguirENaoTiraDoCentro)
{
   const auto entities{makeEntities()};
   for (const Perspective persp : {Perspective::TopDown, Perspective::Lateral}) {
      MapViewState view;
      view.followSelected = true;
      view.perspective = persp;
      applyMapFollow(view, entities, 3);

      const double antes{view.metersPerCell};
      for (int i = 0; i < 6; i++) zoomMap(view, true);
      EXPECT_TRUE(view.followSelected);
      EXPECT_LT(view.metersPerCell, antes);
      applyMapFollow(view, entities, 3);
      EXPECT_TRUE(centradoExatamente(view, entities[3]));

      for (int i = 0; i < 20; i++) zoomMap(view, false);
      EXPECT_TRUE(view.followSelected);
      applyMapFollow(view, entities, 3);
      EXPECT_TRUE(centradoExatamente(view, entities[3]));
   }
}

TEST(MapFollow, GirarNaoDesligaOSeguirENaoTiraDoCentro)
{
   // A rotacao de project() e em torno do PONTO DO PAN -- a entidade
   // centrada tem relN/relE zerados e nao sai do lugar sob nenhum yaw. Por
   // isso girar pode continuar livre junto com o seguir.
   const auto entities{makeEntities()};
   MapViewState view;
   view.followSelected = true;
   applyMapFollow(view, entities, 2);

   for (int i = 0; i < 30; i++) {
      rotateMap(view, i % 3 != 0);
      EXPECT_TRUE(view.followSelected);
      EXPECT_TRUE(centradoExatamente(view, entities[2])) << "giro=" << i;
   }
}

// ---------------------------------------------------------------------------
// 3. Pan manual desliga
// ---------------------------------------------------------------------------

TEST(MapFollow, PanManualDesligaOSeguir)
{
   const auto entities{makeEntities()};
   // Os quatro sentidos das setas, mais o delta de um arrasto -- todos
   // passam por panMap(), que e o unico ponto por onde um gesto manual de
   // mover a vista chega.
   const double gestos[][2]{{-200.0, 0.0}, {200.0, 0.0}, {0.0, 200.0}, {0.0, -200.0}, {37.0, -91.0}};
   for (const auto& g : gestos) {
      for (const Perspective persp : {Perspective::TopDown, Perspective::Lateral}) {
         MapViewState view;
         view.followSelected = true;
         view.perspective = persp;
         applyMapFollow(view, entities, 0);

         panMap(view, g[0], g[1]);
         EXPECT_FALSE(view.followSelected);

         // E, desligado, o proximo quadro NAO reverte o gesto -- e o que
         // torna arrastar/setas de fato utilizavel.
         const double n{view.panNorthM}, e{view.panEastM}, a{view.panAltM};
         EXPECT_FALSE(applyMapFollow(view, entities, 0));
         EXPECT_DOUBLE_EQ(view.panNorthM, n);
         EXPECT_DOUBLE_EQ(view.panEastM, e);
         EXPECT_DOUBLE_EQ(view.panAltM, a);
      }
   }
}

TEST(MapFollow, PanComDeslocamentoZeroTambemDesliga)
{
   // panMap(0,0) so acontece por gesto manual (um Moved sem deslocamento
   // efetivo); desligar mesmo assim mantem a regra simples de enunciar --
   // "mover a vista a mao desliga o seguir" --, sem um limiar escondido.
   MapViewState view;
   view.followSelected = true;
   panMap(view, 0.0, 0.0);
   EXPECT_FALSE(view.followSelected);
}

// ---------------------------------------------------------------------------
// 4. Bordas que o chamador nao precisa tratar
// ---------------------------------------------------------------------------

TEST(MapFollow, DesligadoNaoMexeEmNada)
{
   const auto entities{makeEntities()};
   MapViewState view;
   view.followSelected = false;
   view.panNorthM = 111.0;
   view.panEastM = 222.0;
   view.panAltM = 333.0;

   EXPECT_FALSE(applyMapFollow(view, entities, 0));
   EXPECT_DOUBLE_EQ(view.panNorthM, 111.0);
   EXPECT_DOUBLE_EQ(view.panEastM, 222.0);
   EXPECT_DOUBLE_EQ(view.panAltM, 333.0);
}

TEST(MapFollow, ListaVaziaOuIndiceForaDeFaixaEUmNoOp)
{
   MapViewState view;
   view.followSelected = true;
   view.panNorthM = 111.0;
   view.panEastM = 222.0;
   view.panAltM = 333.0;

   EXPECT_FALSE(applyMapFollow(view, {}, 0));
   EXPECT_FALSE(applyMapFollow(view, {}, -1));

   const auto entities{makeEntities()};
   EXPECT_FALSE(applyMapFollow(view, entities, -1));
   EXPECT_FALSE(applyMapFollow(view, entities, static_cast<int>(entities.size())));
   EXPECT_FALSE(applyMapFollow(view, entities, 99));

   EXPECT_DOUBLE_EQ(view.panNorthM, 111.0);
   EXPECT_DOUBLE_EQ(view.panEastM, 222.0);
   EXPECT_DOUBLE_EQ(view.panAltM, 333.0);
   // Nenhuma dessas bordas pode desarmar o modo -- a lista fica vazia por um
   // quadro toda vez que o cenario e reiniciado.
   EXPECT_TRUE(view.followSelected);
}

TEST(MapFollow, TrocarDeSelecaoTrocaQuemESeguido)
{
   const auto entities{makeEntities()};
   MapViewState view;
   view.followSelected = true;

   applyMapFollow(view, entities, 0);
   EXPECT_TRUE(centradoExatamente(view, entities[0]));
   applyMapFollow(view, entities, 3);
   EXPECT_TRUE(centradoExatamente(view, entities[3]));
   EXPECT_FALSE(centradoExatamente(view, entities[0]));
}

// ---------------------------------------------------------------------------
// 5. O indicador no cabecalho do canvas -- renderizado de verdade
// ---------------------------------------------------------------------------

namespace {

// ARMADILHA MEDIDA, nao suposta: procurar o texto do cabecalho direto no
// Screen::ToString() FALHA no meio da palavra. As linhas de grade sao
// desenhadas ANTES do cabecalho e pintam suas celulas de cinza/dim; o
// Canvas::DrawText de 3 argumentos usa uma funcao de estilo VAZIA (nao
// reseta cor nenhuma), entao o glifo do cabecalho que cai em cima de uma
// coluna de grade mantem a cor dela -- e o ToString() emite a sequencia
// ANSI no MEIO da string. Medido: "rumo=0deg  seguind<ESC>[2m[90mo<ESC>
// [22m[39m=falcon2". O caractere desenhado esta certo; e a serializacao que
// intercala escapes. Por isso o teste compara o texto SEM atributos.
std::string stripAnsi(const std::string& in)
{
   std::string out;
   out.reserve(in.size());
   for (std::size_t i = 0; i < in.size();) {
      if (in[i] == '\x1b') {
         while (i < in.size() && in[i] != 'm') i++;   // pula ate o fim do SGR
         if (i < in.size()) i++;
         continue;
      }
      out.push_back(in[i++]);
   }
   return out;
}

// O caminho da PRODUCAO: DashboardLoop.cpp chama applyMapFollow() e logo em
// seguida renderMap(), com o MESMO indice. Os testes de desenho passam por
// aqui pra nao afirmarem sobre um estado que a aplicacao nunca produz.
std::string renderComFollow(const std::vector<EntityState>& entities, MapViewState& view,
                            const int selectedIndex);

std::string renderMapToString(const std::vector<EntityState>& entities, const MapViewState& view,
                              const int focusedId)
{
   Box box;
   Element el{renderMap(entities, view, focusedId, box, TerrainSampler{})};
   Screen screen{Screen::Create(Dimension::Fixed(140), Dimension::Fixed(40))};
   Render(screen, el);
   return stripAnsi(screen.ToString());
}

// So a LINHA do cabecalho (a que tem "rumo="). O nome de uma entidade
// aparece tambem no rotulo desenhado ao lado da bolinha dela, entao procurar
// na tela inteira confundiria as duas coisas.
std::string headerLine(const std::string& tela)
{
   std::size_t ini{0};
   while (ini < tela.size()) {
      const std::size_t fim{tela.find('\n', ini)};
      const std::string linha{tela.substr(ini, fim == std::string::npos ? fim : fim - ini)};
      if (linha.find("rumo=") != std::string::npos) return linha;
      if (fim == std::string::npos) break;
      ini = fim + 1;
   }
   return {};
}

std::string renderComFollow(const std::vector<EntityState>& entities, MapViewState& view,
                            const int selectedIndex)
{
   applyMapFollow(view, entities, selectedIndex);
   const int focusedId{
      (selectedIndex >= 0 && selectedIndex < static_cast<int>(entities.size()))
         ? entities[static_cast<std::size_t>(selectedIndex)].id : -1};
   return renderMapToString(entities, view, focusedId);
}

} // namespace

// Achado MEDIDO rodando o ./app num terminal de 100 colunas, nao suposto: a
// nota "seguindo=<nome>" invadia a legenda de eixo do canto direito e saia
// "seguindo=falcon1y (NM)" (o "x/" comido). O cabecalho e desenhado DEPOIS
// da legenda, entao ele sobrescreve -- e nome de player e string LIVRE do
// cenario, entao "cabe" nunca foi garantia. Mesma familia da armadilha ja
// documentada pro rotulo "y: alt(ft)".
//
// A afirmacao e DIFERENCIAL de proposito: "ligar o seguir nao pode estragar
// nada que estivesse intacto com ele desligado". Exigir a legenda presente
// em qualquer largura seria exigir demais -- num canvas suficientemente
// estreito o cabecalho BASE (escala + perspectiva + rumo) ja a cobre
// sozinho, o que e anterior a esta feature e nao e o que se esta testando.
TEST(MapFollow, LigarOSeguirNaoEstragaALegendaDeEixoEmNenhumaLargura)
{
   std::vector<EntityState> entities;
   EntityState e;
   e.id = 7;
   e.name = "um-nome-de-player-absurdamente-comprido-vindo-do-cenario";
   entities.push_back(e);

   for (const Perspective persp : {Perspective::TopDown, Perspective::Lateral}) {
      const std::string legenda{persp == Perspective::TopDown ? "x/y (NM)" : "y: alt(ft)"};
      for (int cells = 30; cells <= 130; cells += 5) {
         MapViewState base;
         base.perspective = persp;
         base.canvasWidthPx = cells * 2;
         base.canvasHeightPx = 120;

         MapViewState comFollow{base};
         comFollow.followSelected = true;

         const bool legendaSemFollow{
            renderMapToString(entities, base, 7).find(legenda) != std::string::npos};
         const std::string tela{renderComFollow(entities, comFollow, 0)};

         if (legendaSemFollow) {
            EXPECT_NE(tela.find(legenda), std::string::npos)
               << "o seguir comeu a legenda com " << cells << " celulas, persp="
               << (persp == Perspective::TopDown ? "cima" : "lado");
         }
         // E a escala/rumo nunca sao sacrificados: se algo tem de sair da
         // linha, e a nota do seguir, nunca o cabecalho de sempre.
         EXPECT_NE(tela.find("rumo="), std::string::npos) << "cells=" << cells;
      }
   }
}

TEST(MapFollow, NomeCompridoEAbreviadoNoCabecalhoEmVezDeCortadoEmSilencio)
{
   // Canvas::DrawText descarta em SILENCIO o que passa da borda (IsIn()) --
   // e o cabecalho sobrescreve a legenda antes disso. Abreviar com ".." deixa
   // visivel que houve corte, em vez de exibir um nome que parece outro.
   //
   // A afirmacao e sobre a LINHA DO CABECALHO, nao sobre a tela toda: o nome
   // completo aparece legitimamente em outro lugar do mapa -- no rotulo
   // (callout) desenhado ao lado da bolinha da propria entidade.
   std::vector<EntityState> entities;
   EntityState e;
   e.id = 7;
   e.name = "falcon-com-nome-muito-comprido-mesmo";
   entities.push_back(e);

   MapViewState view;
   view.followSelected = true;
   view.canvasWidthPx = 120;   // 60 celulas: aperta, mas ainda sobra espaco
   view.canvasHeightPx = 120;

   const std::string cab{headerLine(renderComFollow(entities, view, 0))};
   ASSERT_NE(cab.find("seguindo="), std::string::npos) << "cabecalho: [" << cab << "]";
   EXPECT_NE(cab.find(".."), std::string::npos) << "deveria abreviar: [" << cab << "]";
   EXPECT_EQ(cab.find(e.name), std::string::npos) << "nome inteiro nao cabia: [" << cab << "]";
   EXPECT_NE(cab.find("x/y (NM)"), std::string::npos) << "legenda comida: [" << cab << "]";
}

// ACHADO DA REVISAO, medido: nome de player com acento so chega por um
// caminho -- o fantasma DIS, cujo nome e copiado CRU dos 11 bytes de marking
// do PDU. Cortar por BYTE deixava um lead byte solto (UTF-8 invalido), que o
// FTXUI descarta em silencio: o nome exibido virava outro nome. Mesma classe
// da armadilha ja corrigida no editor EDL (utf8GlyphBytes).
TEST(MapFollow, AbreviacaoRespeitaFronteiraDeGlifoUTF8)
{
   std::vector<EntityState> entities;
   EntityState e;
   e.id = 3;
   e.name = "aviao-cacador-ted";   // ASCII, so pra medir a linha de base
   entities.push_back(e);
   const std::string acentuado{"aviao-Ã§Ã£ÃµÃ©Ã­-fim"};
   entities[0].name = acentuado;

   auto utf8Valido = [](const std::string& txt) {
      for (std::size_t i = 0; i < txt.size();) {
         const unsigned char c{static_cast<unsigned char>(txt[i])};
         std::size_t len{1};
         if ((c & 0x80) == 0)            len = 1;
         else if ((c & 0xE0) == 0xC0)    len = 2;
         else if ((c & 0xF0) == 0xE0)    len = 3;
         else if ((c & 0xF8) == 0xF0)    len = 4;
         else                            return false;   // continuation solto
         if (i + len > txt.size()) return false;          // sequencia truncada
         for (std::size_t k = 1; k < len; k++) {
            if ((static_cast<unsigned char>(txt[i + k]) & 0xC0) != 0x80) return false;
         }
         i += len;
      }
      return true;
   };

   // Varre larguras impares E pares -- foi so nas impares que o corte por
   // byte deixava o lead byte solto.
   for (int cells = 40; cells <= 130; cells++) {
      MapViewState view;
      view.followSelected = true;
      view.canvasWidthPx = cells * 2;
      view.canvasHeightPx = 120;

      const std::string cab{headerLine(renderComFollow(entities, view, 0))};
      EXPECT_TRUE(utf8Valido(cab)) << "UTF-8 quebrado no cabecalho com " << cells << " celulas";
   }
}

// ACHADO DA REVISAO, medido: applyMapFollow persegue por INDICE e o
// cabecalho procurava o nome por ID. simulation::AbstractPlayer::id nasce 0,
// entao dois players sem 'id:' no .edl colidem -- e a nota nomeava a
// entidade ERRADA justamente no caso que ela existe pra esclarecer. Hoje o
// nome vem de MapViewState::followLabel, escrito pelo proprio follow.
TEST(MapFollow, ComIdsDUPLICADOSONomeNoCabecalhoEODeQuemEDeFatoSeguido)
{
   std::vector<EntityState> entities;
   for (const char* n : {"bandit1", "falcon9"}) {
      EntityState e;
      e.id = 0;              // os dois sem 'id:' no cenario
      e.name = n;
      e.northM = (std::string{n} == "falcon9") ? 5000.0 : 0.0;
      entities.push_back(e);
   }

   MapViewState view;
   view.followSelected = true;
   view.canvasWidthPx = 240;
   view.canvasHeightPx = 120;

   ASSERT_TRUE(applyMapFollow(view, entities, 1));      // segue 'falcon9'
   EXPECT_EQ(view.followLabel, "falcon9");
   EXPECT_TRUE(centradoExatamente(view, entities[1]));

   const std::string cab{headerLine(renderMapToString(entities, view, entities[1].id))};
   EXPECT_NE(cab.find("seguindo=falcon9"), std::string::npos) << "cabecalho: [" << cab << "]";
   EXPECT_EQ(cab.find("seguindo=bandit1"), std::string::npos) << "nomeou o errado: [" << cab << "]";
}

// ACHADO DA REVISAO: posicao nao-finita (JSBSim divergindo) entrava no pan e
// nao havia volta pela interface -- panMap soma sobre NaN, zoom/giro nao
// tocam o pan. Como o follow roda a cada quadro sem gesto nenhum do usuario,
// um unico quadro ruim envenenaria a vista permanentemente.
TEST(MapFollow, PosicaoNaoFinitaNaoEnvenenaOPan)
{
   const double nan{std::numeric_limits<double>::quiet_NaN()};
   const double inf{std::numeric_limits<double>::infinity()};

   for (int campo = 0; campo < 3; campo++) {
      for (const double ruim : {nan, inf, -inf}) {
         std::vector<EntityState> entities;
         EntityState e;
         e.id = 1;
         e.name = "divergido";
         e.northM = 100.0; e.eastM = 200.0; e.altitudeM = 300.0;
         if (campo == 0) e.northM = ruim;
         if (campo == 1) e.eastM = ruim;
         if (campo == 2) e.altitudeM = ruim;
         entities.push_back(e);

         MapViewState view;
         view.followSelected = true;
         view.panNorthM = 11.0; view.panEastM = 22.0; view.panAltM = 33.0;

         EXPECT_FALSE(applyMapFollow(view, entities, 0));
         EXPECT_DOUBLE_EQ(view.panNorthM, 11.0);
         EXPECT_DOUBLE_EQ(view.panEastM, 22.0);
         EXPECT_DOUBLE_EQ(view.panAltM, 33.0);
         EXPECT_TRUE(view.followLabel.empty());
         // E o modo continua armado -- o quadro ruim e ignorado, nao fatal.
         EXPECT_TRUE(view.followSelected);
      }
   }
}

TEST(MapFollow, CabecalhoMostraQuemEstaSendoSeguido)
{
   const auto entities{makeEntities()};
   MapViewState view;
   view.canvasWidthPx = 240;
   view.canvasHeightPx = 120;

   // Desligado: nenhuma mencao, e o cabecalho de sempre continua la.
   view.followSelected = false;
   const std::string off{renderMapToString(entities, view, entities[1].id)};
   EXPECT_EQ(off.find("seguindo"), std::string::npos);
   EXPECT_NE(off.find("rumo="), std::string::npos);

   // Ligado: o NOME da entidade seguida.
   view.followSelected = true;
   const std::string on{renderComFollow(entities, view, 1)};
   EXPECT_NE(on.find("seguindo=falcon2"), std::string::npos);

   // Ligado sem entidade nenhuma (o que DashboardLoop.cpp passa no quadro em
   // que a lista esta vazia): diz que segue, e e honesto sobre nao ter alvo.
   const std::string semAlvo{renderComFollow({}, view, -1)};
   EXPECT_NE(semAlvo.find("seguindo=--"), std::string::npos);
}
