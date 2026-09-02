#pragma once

#include <string>

namespace app {

//------------------------------------------------------------------------------
// Opcoes de linha de comando desta poc.
//
// Uma unica questao: traduzir argv para uma estrutura. Nada aqui abre
// arquivo, constroi Station nem decide o que fazer com as opcoes -- isso e
// do main.cpp e dos modulos que ele orquestra.
//
// Diferenca das outras pocs: nao ha '-f <arquivo>'. O cenario e sempre um dos
// que o catalogo conhece (ver app/ScenarioCatalog.hpp) -- '-scenario <chave>'
// escolhe um deles direto (usado tanto pela invocacao manual quanto pelo
// reexec de app/Respawn.hpp); sem ele, main.cpp mostra a tela de selecao
// antes de tocar em Station nenhuma.
//
//   -scenario <chave>    pula a tela de selecao, carrega este cenario direto
//   -threads <N>         numTcThreads do pool nativo de tempo critico
//   -deterministic <N>   roda N frames de passo fixo e sai (sem TUI)
//   -parallel-decision   ver app/DeterministicRun.hpp -- so faz sentido com
//                        -deterministic
//------------------------------------------------------------------------------
struct Options
{
   std::string scenarioKey;
   long deterministicFrames{};
   int threadsOverride{};
   bool parallelDecision{};

   bool isDeterministic() const   { return deterministicFrames > 0; }
};

// Aplica sobre 'defaults' o que vier em argv; argumentos desconhecidos sao
// ignorados (mesma tolerancia dos exemplos do framework).
Options parseCommandLine(int argc, char* argv[], const Options& defaults);

} // namespace app
