#pragma once

#include "app/ComponentTreeQuery.hpp"

#include <string>
#include <vector>

//------------------------------------------------------------------------------
// "O que exatamente acontece num passo da simulação" -- a cadeia de chamadas
// REAL do frame MIXR, com os nomes de função, os argumentos e os valores de
// `dt` que de fato circulam, para a aba "Componentes" (F6).
//
// Isto NÃO é um traçado ao vivo (o MIXR é dependência binária, ver o topo do
// CLAUDE.md -- não há como instrumentar `updateTC()` sem recompilar o
// framework). É a estrutura ESTÁTICA do código do framework, lida do fonte
// (`contexts/src/mixr/src/...`, com o arquivo e a linha anotados em cada
// item), preenchida com os NÚMEROS VIVOS da execução: `dt` sai de
// `Station::getTimeCriticalRate()`, o número de repetições sai de
// `getFastForwardRate()`, e `dt0` mostra o efeito da pausa.
//
// A diferença em relação a `app/ComponentFlowState.hpp` é essa: lá é o
// *pulso* que percorre as fases (um relógio de animação); aqui é *o que cada
// fase de fato chama*. Uma responde "onde estamos no ciclo", a outra
// responde "que função roda aí, com que argumento".
//
// Tudo verificado no fonte do fork v1.0.5 antes de escrito -- em especial as
// três coisas que ninguém adivinha lendo só os headers:
//
//   1) `Player::tcFrame()` recebe `dt0/4` (um quarto do dt do frame), e
//      DENTRO de `Player::updateTC()` o `dt4 = dt * 4` devolve o dt do frame
//      inteiro para os módulos que rodam uma vez a cada quatro fases. O
//      mesmo `dt` desce dividido e volta multiplicado.
//   2) `Simulation::updateTC()` faz `execTime += dt` ANTES do teste de
//      freeze -- é por isso que pausar tem de deixar de chamar `tcFrame()`,
//      e não apenas marcar o flag (ver `shared/xclock/ClockStation.cpp`).
//   3) `Component::updateTC()` recorre chamando `obj->tcFrame(dt)` nos
//      filhos, mas `Component::updateData()` chama `obj->updateData(dt)`
//      DIRETO -- não há `bgFrame()`. As duas recursões não são simétricas.
//------------------------------------------------------------------------------
namespace app {

enum class CallLineKind {
   Thread,   // de que thread parte a cadeia -- cabeçalho, não é uma chamada
   Call,     // uma chamada de função
   Loop,     // um for/switch que muda quantas vezes o filho roda
   Assign,   // uma atribuição que muda o valor que desce (dt0, dt4)
   Note,     // consequência/armadilha, sem código correspondente
};

struct CallChainLine
{
   int depth{};                 // indentação (0 = raiz da cadeia)
   CallLineKind kind{CallLineKind::Call};
   std::string text;            // "Simulation::updateTcPlayerList"
   std::string args;            // "(players, dt0/4 = 0.005000 s, idx, n)"
   std::string note;            // "n = numTcThreads -- cada thread pega 1 player a cada n"
   std::string sourceRef;       // "Simulation.cpp:596" -- onde conferir
   bool active{};               // é a linha que corresponde à fase corrente
};

// Os números VIVOS que entram nos argumentos. Lidos da Station em execução
// (ver DashboardLoop.cpp) -- não são constantes.
struct FrameCallParams
{
   double tcRateHz{50.0};
   double bgRateHz{10.0};
   unsigned int fastForwardRate{1};
   int numTcThreads{1};
   bool paused{};
};

// A cadeia do frame de TEMPO CRÍTICO (fases 0..3) ou a de FUNDO
// (updateData), conforme a fase pedida. As linhas marcadas 'active' são as
// que essa fase de fato executa.
std::vector<CallChainLine> buildFrameCallChain(EstimatedPhase phase, const FrameCallParams& params);

// true quando a fase é uma das quatro do frame de tempo crítico (ou o
// contorno estrutural dele) -- isto é, quando buildFrameCallChain() devolve
// a cadeia T/C e não a de fundo. Usado pelo cabeçalho do painel.
bool isTimeCriticalPhase(EstimatedPhase phase);

// O CAMINHO DA DESCIDA até a chamada desta fase: as linhas marcadas
// `active` mais todos os ancestrais delas (as linhas anteriores de
// profundidade estritamente decrescente). É a cadeia inteira menos os ramos
// que esta fase não toma -- curta o bastante para caber no card de detalhe,
// que é onde ela mora agora.
std::vector<CallChainLine> frameDescentPath(const std::vector<CallChainLine>& chain);

// O rótulo CURTO da chamada que um nó desta fase executa AGORA, para ser
// desenhado ao lado do próprio nó no canvas -- "dynamics(0.020s)",
// "process(0.020s)". Vazio quando o nó não participa da fase corrente (a
// esmagadora maioria, em qualquer instante), e vazio também para os nós
// estruturais fora do contorno do frame.
//
// É a resposta gráfica a "que função é chamada aqui, com que argumento":
// fica NO DESENHO, junto do componente que a executa, em vez de num painel
// de texto separado que roubaria espaço da árvore.
std::string nodeCallLabel(EstimatedPhase nodePhase, EstimatedPhase flowPhase,
                          const FrameCallParams& params);

// O `dt` de um frame de tempo crítico, em segundos: 1 / tcRate. Zero (e não
// infinito) quando a taxa é inválida -- é o valor que o passo manual usa,
// e chamar tcFrame(inf) seria bem pior do que não dar o passo.
double frameStepSeconds(const FrameCallParams& params);

} // namespace app
