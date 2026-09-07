#pragma once

#include <string>
#include <vector>

namespace app {

//------------------------------------------------------------------------------
// Um item navegavel generico: 'key' e o que a tela devolve na escolha,
// 'label' e o que aparece na lista, 'description' e o texto de apoio abaixo
// dela (ver kPickerDescLines no .cpp). Hoje a UNICA fonte e a pasta de
// sandbox de '-folder <pasta>' (app/ScenarioFolder.hpp) -- qualquer lista de
// "nome + descricao" pode virar uma tela com isto, sem duplicar FTXUI.
//------------------------------------------------------------------------------
struct PickerItem
{
   std::string key;
   std::string label;
   std::string description;
};

// A tela GENERICA -- mostrada ANTES de qualquer Station existir. Bloqueia
// ate o usuario escolher (Enter/clique) ou sair (q/Esc/Ctrl+C). Devolve o
// 'key' do item escolhido, ou string vazia se o usuario saiu sem escolher.
std::string runPickerScreen(const std::vector<PickerItem>& items, const std::string& title);

} // namespace app
