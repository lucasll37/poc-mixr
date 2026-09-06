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
// Tres formas de escolher o cenario, e elas se excluem:
//
//   -scenario <chave>    uma das chaves do catalogo (ver app/ScenarioCatalog.hpp)
//                        -- e por aqui que as POCS rodam, ja que o ./app e o
//                        runner unico delas. Tambem e o que o reexec de
//                        app/Respawn.hpp usa.
//   -f <arquivo>         um .edl/.edl.in qualquer, fora do catalogo -- o caso
//                        das fixtures de teste (tests/scenario/make_fixture.py).
//   -folder <pasta>      uma pasta navegavel de cenarios de sandbox, formato
//                        '<pasta>/<cenario>/configs/*.edl' (ver
//                        app/ScenarioFolder.hpp) -- QUANDO COMBINADO com
//                        '-scenario <nome>', este ultimo deixa de ser uma
//                        chave do catalogo e vira o NOME DA SUBPASTA dentro
//                        de <pasta>, pulando a tela de navegacao (util para
//                        automatizar/testar). Sem '-scenario', abre a tela.
//
// Sem nenhum dos tres, main.cpp RECUSA com um erro -- e obrigatorio passar
// um deles explicitamente (uso normal desta aplicacao nunca "adivinha" o
// que abrir).
//
//   -threads <N>         numTcThreads do pool nativo de tempo critico
//   -deterministic <N>   roda N frames de passo fixo e sai (sem TUI)
//   -parallel-decision   ver app/DeterministicRun.hpp -- so faz sentido com
//                        -deterministic
//
//   -internal-picker     NAO E USO NORMAL -- existe so para o REEXEC interno
//                        de app/Respawn.hpp quando o usuario pede "carregar
//                        outro cenario"/"parar" de dentro do TUI (ver
//                        DashboardExit::ChangeScenario em main.cpp): sem
//                        nenhuma chave/arquivo/pasta pra abrir de novo, o
//                        processo respawnado precisa de ALGUM jeito de
//                        chegar na tela de selecao do catalogo -- essa flag
//                        e esse jeito, sem reabrir a porta de "rodar sem
//                        nada" pra quem invoca o binario de fora.
//------------------------------------------------------------------------------
struct Options
{
   std::string scenarioKey;
   std::string scenarioPath;     // '-f <arquivo>' -- vazio quando nao usado
   std::string scenarioFolder;   // '-folder <pasta>' -- vazio quando nao usado
   long deterministicFrames{};
   int threadsOverride{};
   bool parallelDecision{};
   bool internalPicker{};        // '-internal-picker' -- ver o comentario acima

   bool isDeterministic() const   { return deterministicFrames > 0; }
};

// Aplica sobre 'defaults' o que vier em argv; argumentos desconhecidos sao
// ignorados (mesma tolerancia dos exemplos do framework).
Options parseCommandLine(int argc, char* argv[], const Options& defaults);

} // namespace app
