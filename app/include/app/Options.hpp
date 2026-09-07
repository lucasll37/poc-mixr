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
//   -f <arquivo>         um .edl/.edl.in qualquer -- o caso das fixtures de
//                        teste (tests/scenario/make_fixture.py) e de rodar
//                        um cenario apontando direto pro arquivo. Assume a
//                        frota falcon1..4 (ver app::adHocScenario()).
//   -folder <pasta>      uma pasta navegavel de cenarios de sandbox, formato
//                        '<pasta>/<cenario>/configs/*.edl' (ver
//                        app/ScenarioFolder.hpp). Combinado com
//                        '-scenario <nome>', este ultimo e o NOME DA
//                        SUBPASTA dentro de <pasta>, pulando a tela de
//                        navegacao (util para automatizar/testar). Sem
//                        '-scenario', abre a tela. A frota e descoberta em
//                        runtime (app::discoverFleet()), nao assumida.
//
// Sem nenhuma das duas, main.cpp RECUSA com um erro -- e obrigatorio passar
// uma delas explicitamente (uso normal desta aplicacao nunca "adivinha" o
// que abrir).
//
//   -threads <N>         numTcThreads do pool nativo de tempo critico
//   -deterministic <N>   roda N frames de passo fixo e sai (sem TUI)
//   -parallel-decision   ver app/DeterministicRun.hpp -- so faz sentido com
//                        -deterministic
//------------------------------------------------------------------------------
struct Options
{
   std::string scenarioKey;      // '-scenario <nome>' -- so tem efeito combinado com '-folder'
   std::string scenarioPath;     // '-f <arquivo>' -- vazio quando nao usado
   std::string scenarioFolder;   // '-folder <pasta>' -- vazio quando nao usado
   long deterministicFrames{};
   int threadsOverride{};
   bool parallelDecision{};

   bool isDeterministic() const   { return deterministicFrames > 0; }
};

// Aplica sobre 'defaults' o que vier em argv; argumentos desconhecidos sao
// ignorados (mesma tolerancia dos exemplos do framework).
Options parseCommandLine(int argc, char* argv[], const Options& defaults);

} // namespace app
