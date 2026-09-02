#pragma once

#include "app/DashboardState.hpp"

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>

#include <string>

//------------------------------------------------------------------------------
// A aba "Frota": uma linha por entidade (para a lista ROLAVEL da esquerda --
// ver DashboardLoop.cpp, que usa ftxui::Menu com estas funcoes no
// 'entries_option.transform', o motivo de nao ser este arquivo a montar o
// Component: o Menu precisa referenciar estado mutavel do laco) mais o
// detalhe completo (o box de sempre, agora generico -- ver EntityState) da
// entidade selecionada.
//
// Todas as cores/rotulos aqui vem de campos GENERICOS de Player
// (majorType/side/mode/behaviorLabel) -- nenhuma suposicao de que a entidade
// e uma aeronave do modelo 'flight'. Ver o cabecalho de app/DashboardState.hpp.
//------------------------------------------------------------------------------
namespace app {

// Altura (em linhas de terminal) do painel de detalhe -- FIXA nas duas
// abas (Frota e Mapa) e ao navegar entre players (pedido explicito:
// "deve ter tamanho fixo ao se navegar entre players e entre abas"), pra
// nao "pular" quando um campo opcional aparece/some ou a arvore muda de
// tamanho. Generosa o bastante pra caber o card mais cheio (todos os
// campos + arvore + status de breakpoint) sem cortar.
const int kDetailPanelHeight{34};

// A LARGURA do painel de detalhe e calculada em runtime, nao e uma
// constante -- ver 'detailPanelWidth' em DashboardLoop.cpp: pedido
// explicito, "torne-o mais largo, ocupando por referencia ate onde o mapa
// acaba" (kMapCanvasWidthCells, de app/MapPanel.hpp) usando o tamanho REAL
// do terminal (ftxui::Terminal::Size()). Continua sendo a MESMA largura
// nas duas abas -- so deixou de ser um numero fixo escrito aqui.
const int kDetailPanelMinWidth{40};
const int kDetailPanelMaxWidth{100};

// Largura fixa de cada COLUNA da linha compacta da lista (Frota) -- ver
// renderEntityRow(). Alinhar em colunas de verdade (em vez de texto colado
// com padding ad-hoc) e o que garante que a mesma coluna sempre cai
// embaixo da anterior, entidade apos entidade.
const int kColBadge{3};
const int kColName{11};
const int kColType{12};
const int kColBehavior{10};
const int kColThread{7};
const int kColAlt{10};
const int kColSpd{9};
const int kColFuel{9};

// BLUE/RED/YELLOW/CYAN/GRAY/WHITE (mixr::models::Player::Side) -- cor de lado,
// nao de comportamento: e o mesmo conceito de amigo/inimigo/neutro que
// qualquer modelo (aviao, navio, veiculo terrestre...) ja declara no EDL.
ftxui::Color sideColor(unsigned int side);
std::string sideLabel(unsigned int side);

// AIR_VEHICLE/GROUND_VEHICLE/WEAPON/SHIP/SPACE_VEHICLE/BUILDING/LIFE_FORM/
// GENERIC (mixr::models::Player::MajorType) -- um glifo por "especie" de
// player, generico a qualquer modelo.
std::string majorTypeGlyph(unsigned int majorType);

// simulation::AbstractPlayer::Mode (ACTIVE/CRASHED/DETONATED/...).
std::string modeLabel(int mode);
ftxui::Color modeColor(int mode);

// Cor do rotulo de comportamento (bt=) -- hash deterministico sobre uma
// paleta fixa, DE PROPOSITO sem switch por nome: o vocabulario de uma arvore
// de comportamento e do MODELO, e um modelo futuro pode usar rotulos que
// este arquivo nunca viu. "--" (sem decisao ainda) fica sempre cinza.
ftxui::Color behaviorColor(const std::string& label);

// A linha compacta da lista (usada como fallback de largura do ftxui::Menu
// -- a cor de verdade vem de renderEntityRow(), no transform).
std::string entityRowText(const EntityState& e);

// A MESMA linha, com cor -- chamada de dentro do 'entries_option.transform'
// do Menu em DashboardLoop.cpp, uma vez por entidade visivel.
ftxui::Element renderEntityRow(const EntityState& e, bool focused);

// Cabecalho da lista -- um rotulo por coluna, nos MESMOS 'size(WIDTH, EQUAL,
// kCol*)' de renderEntityRow(), pra cair exatamente em cima da coluna que
// nomeia (ex.: "thread", em cima da coluna que hoje so mostra "T0"/"-" --
// sem isso nao da pra saber o que aquele numero quer dizer so olhando a
// lista). Desenhado uma vez, fora do ftxui::Menu (que rola) -- fica fixo no
// topo da aba Frota, ver DashboardLoop.cpp.
ftxui::Element renderEntityListHeader();

// O painel de detalhe completo (o box de sempre) de UMA entidade -- as
// linhas exclusivas de AirVehicle (combustivel/G/empuxo/pista) so aparecem
// quando o campo 'has*' correspondente esta ligado.
ftxui::Element renderEntityDetail(const EntityState& e);

} // namespace app
