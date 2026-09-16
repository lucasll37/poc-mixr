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
//   -file <arquivo>      um .edl/.edl.in qualquer -- o caso das fixtures de
//                        teste (tests/scenario/make_fixture.py) e de rodar
//                        um cenario apontando direto pro arquivo. Assume a
//                        frota falcon1..4 (ver app::adHocScenario()) -- nao
//                        descobre os players do arquivo, so aponta pra ele.
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
//   -numTcThreads <N>    numTcThreads do pool nativo de tempo critico
//                        (era '-threads' -- renomeado para nao contradizer
//                        o nome de '-numBgThreads' abaixo, o par que ela
//                        sempre teve no framework, ver Simulation::
//                        setSlotNumTcThreads()/setSlotNumBgThreads())
//   -numBgThreads <N>    numBgThreads do pool nativo de background (o laco
//                        de updateData(), fora do frame de tempo critico).
//                        Sem a flag, o default e' 2 -- ao contrario do T/C
//                        (que default para METADE dos nucleos), o laco de
//                        background deste projeto nunca teve motivo para
//                        variar tanto; 2 so' exercita o mecanismo nativo
//                        sem monopolizar a maquina. Mesmo clamp do T/C:
//                        [1, nucleos-1].
//   -deterministic <N>   roda N frames de passo fixo e sai (sem TUI)
//------------------------------------------------------------------------------
struct Options
{
   std::string scenarioKey;      // '-scenario <nome>' -- so tem efeito combinado com '-folder'
   std::string scenarioPath;     // '-file <arquivo>' -- vazio quando nao usado
   std::string scenarioFolder;   // '-folder <pasta>' -- vazio quando nao usado
   long deterministicFrames{};
   int tcThreadsOverride{};      // '-numTcThreads <N>' -- 0 == "sem override" (usa o default)
   int bgThreadsOverride{};      // '-numBgThreads <N>' -- 0 == "sem override" (usa o default)

   bool isDeterministic() const   { return deterministicFrames > 0; }
};

// Aplica sobre 'defaults' o que vier em argv; argumentos desconhecidos sao
// ignorados (mesma tolerancia dos exemplos do framework).
Options parseCommandLine(int argc, char* argv[], const Options& defaults);

} // namespace app
