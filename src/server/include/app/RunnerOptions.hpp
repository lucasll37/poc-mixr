#pragma once

#include <string>

namespace app {

//------------------------------------------------------------------------------
// Opcoes de linha de comando do sim-runner. So tres, e as tres sao
// obrigatorias na pratica (o server sempre as passa) -- nao existe modo
// "tempo real" aqui, ao contrario das outras pocs: o sim-runner SEMPRE roda
// passo fixo e sai.
//
//   -f <arquivo>       o corpo enviado pelo cliente em POST /simulate (so o
//                       conteudo de 'players: {}' -- ver ScenarioAssembler)
//   -frames <N>         quantos frames de passo fixo rodar
//   -threads <N>        numTcThreads do pool nativo de tempo critico
//------------------------------------------------------------------------------
struct RunnerOptions
{
   std::string bodyPath;
   long frames{600};
   int threadsOverride{};
};

// Encerra o processo se '-f' nao vier ou '-frames' nao for positivo -- sem
// isso nao ha o que montar.
RunnerOptions parseRunnerCommandLine(int argc, char* argv[]);

} // namespace app
