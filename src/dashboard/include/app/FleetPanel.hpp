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

// Largura (em colunas de terminal) do painel de detalhe -- a MESMA nas
// abas Frota e Mapa (pedido explicito: "essa aba deve ter a mesma largura
// nas abas frota e mapa"), tomando a largura que a Frota ja usava como
// referencia. Ver DashboardLoop.cpp: aplicado com 'size(WIDTH, EQUAL, ...)'
// nos dois lugares em vez de deixar o 'flex' decidir (que daria larguras
// DIFERENTES em cada aba, por terem vizinhos diferentes na mesma linha).
const int kDetailPanelWidth{40};

// Largura fixa de cada COLUNA da linha compacta da lista (Frota) -- ver
// renderEntityRow(). Alinhar em colunas de verdade (em vez de texto colado
// com padding ad-hoc) e o que garante que a mesma coluna sempre cai
// embaixo da anterior, entidade apos entidade.
const int kColBadge{3};
const int kColName{11};
const int kColType{12};
const int kColBehavior{10};
const int kColThread{6};
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

// O painel de detalhe completo (o box de sempre) de UMA entidade -- as
// linhas exclusivas de AirVehicle (combustivel/G/empuxo/pista) so aparecem
// quando o campo 'has*' correspondente esta ligado.
ftxui::Element renderEntityDetail(const EntityState& e);

} // namespace app
