#pragma once

#include <string>

namespace app {

//------------------------------------------------------------------------------
// A tela de selecao de cenario -- mostrada ANTES de qualquer Station existir
// (main.cpp so chama app::buildStation() depois que isto devolve uma chave).
// Bloqueia ate o usuario escolher (Enter) ou sair (q/Esc/Ctrl+C).
//
// Devolve a chave escolhida (ver app/ScenarioCatalog.hpp), ou string vazia
// se o usuario saiu sem escolher.
//------------------------------------------------------------------------------
std::string runScenarioPicker();

} // namespace app
