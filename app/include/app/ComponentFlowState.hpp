#pragma once

#include "app/ComponentTreeQuery.hpp"

#include <cstddef>

//------------------------------------------------------------------------------
// SEGUNDA METADE da feature "Componentes" (F6) -- ver o comentario grande
// no topo de app/ComponentTreeQuery.hpp/app/ComponentTreePanel.hpp para a
// PRIMEIRA (arvore estatica, navegavel). Isto acrescenta a "animacao de
// fluxo": um pulso que percorre, em ordem fixa, as fases do ciclo do frame
// MIXR documentado no CLAUDE.md (secao "O modelo MIXR em uma tela") --
// fase 0 dynamics -> fases 1/2 sensor -> fase 3 decisao -> decisao em
// updateData() -> updateData() de fundo -> volta pra fase 0.
//
// ISTO NAO E UMA MEDICAO. O MIXR e dependencia BINARIA (ver o topo do
// CLAUDE.md) -- nao ha como instrumentar de verdade cada chamada de
// updateTC()/updateData() dentro do framework sem recompila-lo, o que esta
// fora de cogitacao. 'EstimatedPhase' (app/ComponentTreeQuery.hpp) ja e uma
// HEURISTICA sobre nome de slot/classe; o "pulso" desta animacao anda por
// um relogio de ANIMACAO PROPRIO (steps por segundo, avancado a cada
// REDESENHO da UI -- ver tickComponentFlowAnimation()), NUNCA tentando
// sincronizar com o timing real do frame de tempo critico, que roda a ate
// 50 Hz -- rapido demais pra uma UI que amostra a ~10 Hz observar de
// verdade (mesma razao pela qual o resto do dashboard so mede o que da pra
// medir e chama heuristica de heuristica -- ver o aviso "(ESTIMADO)" no
// card de detalhe). E um MODELO CONCEITUAL do ciclo, para ensinar/mostrar a
// ORDEM das fases -- nao um tracado ao vivo de chamadas.
//------------------------------------------------------------------------------
namespace app {

// Ordem ciclica FIXA do "ciclo conceitual". 'Structural' abre o ciclo (nao
// "roda" em fase nenhuma -- e onde Station/WorldModel/Player orquestram as
// quatro fases -- mas e o ponto de partida visual mais natural: tudo
// comeca e termina passando por ali). 'Unknown' fica DE FORA -- nenhum no
// de verdade "esta" nela, e' so o fallback de estimatePhase() quando a
// heuristica nao reconhece nada.
inline constexpr EstimatedPhase kComponentFlowCycle[]{
   EstimatedPhase::Structural,
   EstimatedPhase::DynamicsPhase0,
   EstimatedPhase::SensorPhase1And2,
   EstimatedPhase::DecisionPhase3,
   EstimatedPhase::DecisionBackground,
   EstimatedPhase::Background,
};
inline constexpr std::size_t kComponentFlowCycleLen{6};

// Estado da animacao -- mantido pelo CHAMADOR (app/DashboardLoop.cpp),
// mesmo espirito de ComponentTreeViewState/MapViewState. Deliberadamente
// pequeno e sem nenhum campo de tempo de PAREDE: o relogio e so uma
// contagem de redesenhos (ver tickComponentFlowAnimation()).
struct ComponentFlowState
{
   std::size_t cycleIndex{};     // indice em kComponentFlowCycle
   bool playing{true};
   int stepsPerSecond{1};        // 1, 2 ou 4 -- ver cycleComponentFlowSpeed()
   int redrawsSincePlay{};       // contador de REDESENHOS desde o ultimo passo
};

EstimatedPhase currentFlowPhase(const ComponentFlowState& flow);

// Avanca uma fase, dando a volta no fim do ciclo -- chamada tanto pelo
// passo manual ([n], funciona mesmo tocando, embora so faca sentido de
// verdade pausado) quanto pelo tick automatico de reproducao.
void advanceComponentFlowStep(ComponentFlowState& flow);

// A reproducao da animacao e ESCRAVA da simulacao: ela toca enquanto a
// simulacao roda e para quando ela pausa (ver DashboardLoop.cpp, que chama
// isto a cada redesenho com '!paused'). Nao ha mais um toggle proprio --
// tinha o efeito ruim de deixar os dois relogios divergirem, e o pedido foi
// justamente que o controle da aba F6 mexesse na simulacao de verdade.
void setComponentFlowPlaying(ComponentFlowState& flow, bool playing);

// 1x -> 2x -> 4x -> 1x -- mesma escada pequena do resto do app (ver
// shared/xclock/TimeControls, embora esta seja independente daquela: o
// relogio da simulacao e o desta animacao NAO tem nenhuma relacao).
void cycleComponentFlowSpeed(ComponentFlowState& flow);

// Chamada a CADA redesenho do dashboard -- reaproveita o mesmo pulso de
// redesenho que 'simThread' ja dispara a ~10 Hz via
// screen.PostEvent(Event::Custom) (ver o comentario grande no topo de
// DashboardLoop.cpp) como relogio de animacao, em vez de medir tempo de
// parede: conta quantos redesenhos se passaram desde o ultimo avanco e
// avanca quando esse numero cobre 'redrawsPerSecond / stepsPerSecond'.
// No-op quando pausado ('playing == false'). 'redrawsPerSecond' e um
// parametro (nao uma constante) so pra o teste poder simular uma taxa
// controlada sem esperar tempo de parede de verdade.
void tickComponentFlowAnimation(ComponentFlowState& flow, int redrawsPerSecond = 10);

} // namespace app
