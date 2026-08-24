#ifndef __xclock_ClockStation_H__
#define __xclock_ClockStation_H__

#include "mixr/simulation/Station.hpp"

namespace mixr {
namespace xclock {

//------------------------------------------------------------------------------
// Class: ClockStation
//
// Description: Station com controle de VELOCIDADE do tempo simulado --
//              acelerar, frear (camara lenta) e pausar.
//
// Factory name: ClockStation
//
// Slots: nenhum proprio -- a velocidade inicial continua sendo o slot NATIVO
//        'fastForwardRate' da Station (ver o .epp do cenario).
//
// DIVISAO DELIBERADA entre o que e nativo e o que e nosso:
//
//   * ACELERAR (>= 1x) e 100% NATIVO. Station::processTimeCriticalTasks()
//     faz `for (jj=0; jj < getFastForwardRate(); jj++) tcFrame(dt);`
//     (Station.cpp:506-511), ou seja, a cada periodo real da thread T/C o
//     tempo simulado avanca N frames. setFastForwardRate() e publico e
//     virtual, entao muda em runtime sem nenhuma ginastica.
//
//   * FREAR (< 1x) NAO EXISTE no framework: 'fastForwardRate' e
//     unsigned int, so multiplica -- nao ha como pedir "meio frame". Nao ha
//     tambem como baixar a taxa da thread T/C em runtime: Station so expoe
//     getTimeCriticalRate() (o setter e slot privado) e o rate da
//     base::PeriodicThread e fixado na construcao. Dai a UNICA coisa que
//     esta classe acrescenta: um override de processTimeCriticalTasks() que,
//     abaixo de 1x, roda um frame com o dt ENCURTADO. Passo de integracao
//     menor -- nunca maior -- entao a dinamica nao degrada (JSBSim inclusive).
//
//   * PAUSAR e NATIVO, por um caminho que nao e obvio: nao existe
//     "Simulation::pause()". O que existe e o flag de freeze do
//     base::Component, e quem o honra e Simulation::updateTC()/updateData(),
//     com `if (isFrozen()) dt0 = 0.0` (Simulation.cpp:498 e 625). O freeze
//     NAO se propaga para os filhos por chamada; a cascata acontece por
//     CONSULTA, no sentido inverso: Player::isFrozen() testa o proprio flag
//     OU o da simulacao (Player.cpp:445-448), System::isFrozen() testa o
//     proprio OU o do ownship (System.cpp:52-56), e Player::dynamics()
//     repassa isFrozen() ao DynamicsModel (Player.cpp:2773) -- o
//     JSBSimModel entao poe a JSBSim em hold (JSBSimModel.cpp:657).
//     Resultado: congelar a SIMULACAO congela players, sistemas e o 6-DOF
//     de uma vez. Por isso setPaused() age em getSimulation(), e nao na
//     Station -- congelar a Station nao pararia nada disso.
//
//     O freeze sozinho, porem, NAO para o relogio de execucao: veja o
//     comentario em processTimeCriticalTasks() (ClockStation.cpp) para a
//     ordem invertida dentro de Simulation::updateTC() que obriga a pausa a
//     tambem deixar de chamar tcFrame().
//
// EFEITO NO TACVIEW/GRAVADOR, de graca: o xtacview::TacviewOutput datava as
// linhas ACMI com pb::Time::exec_time (TacviewOutput.cpp:373), que vem de
// Simulation::getExecTimeSec(). Como o tempo simulado congela junto com o
// freeze e acelera junto com o fastForwardRate, o replay acompanha as tres
// operacoes sem uma linha de codigo a mais.
//
// LIMITE CONHECIDO: os agentes UBF (models::SimAgent) sao componentes da
// STATION, e ubf::Agent::updateData() chama controller(dt) sem consultar
// isFrozen() (Agent.cpp:59-62). Com a simulacao pausada eles continuam
// avaliando -- so que sobre um mundo estatico, reemitindo o mesmo comando
// para um autopilot congelado. Nada se move; a decisao apenas nao para.
//------------------------------------------------------------------------------
class ClockStation : public simulation::Station
{
   DECLARE_SUBCLASS(ClockStation, simulation::Station)

public:
   ClockStation();

   // Velocidade corrente, em multiplos do tempo real (0.25 = camara lenta,
   // 1.0 = tempo real, 8.0 = oito vezes mais rapido).
   double getTimeScale() const;

   // Ajusta a velocidade. Valores >= 1 vao para o fastForwardRate nativo
   // (arredondados para inteiro, que e o que o slot aceita); valores < 1
   // ligam a camara lenta. Retorna false se estiver fora da faixa.
   bool setTimeScale(const double);

   bool isPaused() const;
   bool setPaused(const bool);
   bool togglePaused();

   static double getMinTimeScale();
   static double getMaxTimeScale();

protected:
   void processTimeCriticalTasks(const double dt) override;

private:
   // 1.0 = sem camara lenta (a velocidade mora no fastForwardRate nativo).
   double slowFactor{1.0};
};

}
}

#endif
