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
// Duas formas de escolher o cenario, e elas se excluem:
//
//   -scenario <chave>    uma das chaves do catalogo (ver app/ScenarioCatalog.hpp)
//                        -- e por aqui que as POCS rodam, ja que o ./app e o
//                        runner unico delas. Tambem e o que o reexec de
//                        app/Respawn.hpp usa.
//   -f <arquivo>         um .edl/.edl.in qualquer, fora do catalogo -- o caso
//                        das fixtures de teste (tests/scenario/make_fixture.py).
//
// Sem nenhum dos dois, main.cpp mostra a tela de selecao antes de tocar em
// Station nenhuma.
//
//   -threads <N>         numTcThreads do pool nativo de tempo critico
//   -deterministic <N>   roda N frames de passo fixo e sai (sem TUI)
//   -parallel-decision   ver app/DeterministicRun.hpp -- so faz sentido com
//                        -deterministic
//------------------------------------------------------------------------------
struct Options
{
   std::string scenarioKey;
   std::string scenarioPath;   // '-f <arquivo>' -- vazio quando nao usado
   long deterministicFrames{};
   int threadsOverride{};
   bool parallelDecision{};

   bool isDeterministic() const   { return deterministicFrames > 0; }
};

// Aplica sobre 'defaults' o que vier em argv; argumentos desconhecidos sao
// ignorados (mesma tolerancia dos exemplos do framework).
Options parseCommandLine(int argc, char* argv[], const Options& defaults);

} // namespace app
