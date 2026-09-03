#pragma once

#include "app/DashboardState.hpp"

#include <ftxui/dom/elements.hpp>

//------------------------------------------------------------------------------
// A aba "Tempo Nao-Critico" (F4): o que roda na thread de tempo NAO critico -- ver o
// comentario grande de app::BackgroundInfo em DashboardState.hpp para o
// "porque" (este `app` nunca cria a StationBgPeriodicThread nativa; quem faz
// esse papel e o proprio laco de simThread em DashboardLoop.cpp).
//
// Um unico painel estatico (sem lista rolavel -- nao ha "N entidades" aqui,
// so um punhado de contadores/taxas), no mesmo estilo de titulo+borda do
// card de detalhe das outras abas.
//------------------------------------------------------------------------------
namespace app {

ftxui::Element renderBackgroundPanel(const BackgroundInfo& bg);

} // namespace app
