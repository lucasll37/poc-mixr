#pragma once

#include "app/MetaObjectSnapshot.hpp"

#include <ftxui/dom/elements.hpp>

#include <string>

//------------------------------------------------------------------------------
// A aba "Memoria": uma linha por classe amostrada em app/MetaObjectSnapshot,
// com contagem AO VIVO, pico, total criado, uma BARRA (count/pico) e o
// veredito de crescimento sustentado -- ja agnostico a modelo por
// construcao (a fonte, xplugin::pluginMetaObjects(), e por isso o header de
// MetaObjectSnapshot.hpp).
//------------------------------------------------------------------------------
namespace app {

// Largura fixa de cada coluna -- mesmo raciocinio de app/FleetPanel.hpp
// (pedido explicito de alinhamento tabular): cada campo em 'size(WIDTH,
// EQUAL, N)', nao texto colado com padding.
const int kColSource{3};
const int kColFactory{28};
const int kColCount{10};
const int kColPeak{9};
const int kColCreated{12};
const int kColBar{20};

// Texto plano de uma linha -- usado como fallback de largura do
// ftxui::Menu em DashboardLoop.cpp (a cor de verdade vem de
// renderClassRow(), no transform, mesmo padrao de app/FleetPanel.hpp).
std::string classRowText(const ClassStat& s);

ftxui::Element renderClassRow(const ClassStat& s, bool focused);

} // namespace app
