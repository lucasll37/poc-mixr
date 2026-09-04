#pragma once

#include "xrlbridge/RLBridge.hpp"

#include <string>
#include <utility>

namespace mixr { namespace simulation { class Station; } }

namespace rl {

//------------------------------------------------------------------------------
// A classe que a ponte pybind11 (PyBindings.cpp) expoe a Python. Constroi UMA
// Station por processo e a mantem viva entre chamadas de reset()/step() -- e
// esse "processo Python fica de pe" que da ao gymnasium.Env a latencia baixa
// que pybind11 embutido promete (sem round-trip de rede por passo).
//
// V1 -- UM UNICO agente RL por processo, sem chave por player id (ver o
// mesmo limite em shared/xrlbridge/RLBridge.hpp e
// models/flight/include/ubf/RLBridgeBehavior.hpp).
//
// 'playerName' TEM DE SER O MESMO PLAYER configurado com
// ( RLBridgeBehavior ) no .epp (default: falcon1) -- BUG CONFIRMADO E
// CORRIGIDO: como shared/xrlbridge nao tem chave por player id, o
// Command/Observation trocados por step()/reset() sao SEMPRE os do player
// que o .epp escolheu, nao os do 'playerName' passado aqui. Um valor
// diferente (typo, ou um player que existe mas nao e o configurado com
// RLBridgeBehavior -- ex.: falcon2..4 em src/rl/configs/scenario_rl.epp)
// so afeta a checagem de 'terminated' (player->isCrashed()) em step(), que
// passava a olhar um player TOTALMENTE DESLIGADO do Command/Observation
// reais -- silenciosamente, sem erro nenhum, treinando contra um sinal de
// termino que nao correspondia a aeronave de verdade sendo controlada.
// reset() agora falha alto (std::runtime_error) se o player nao existir no
// cenario; nao ha como validar daqui que e o MESMO player com
// RLBridgeBehavior -- esse tipo mora no plugin do modelo, que este host
// nao pode conhecer (tests/guard/check_host_opaco.sh).
//
// SO PODE EXISTIR UMA Station POR PROCESSO -- CONFIRMADO, nao e mais um
// risco hipotetico. shared/xplugin sela o registro de plugins depois do
// PRIMEIRO edl_parser() (mixr::xplugin::seal(), dentro de buildStation());
// um SEGUNDO NativeSimulation no mesmo processo, ao chamar reset() pela
// primeira vez, cai em buildStation() -> edl_parser() de novo e o registro
// recusa com "loadModule(...) depois do parse". Medido rodando: um script
// Python que cria dois MixrFlightEnv (dois NativeSimulation) no mesmo
// processo aborta no reset() do segundo. Trocar de cenario/reiniciar do
// zero exige um processo novo (Python multiprocessing, ou reexec) -- o
// MESMO raciocinio que ja levou app/Respawn.hpp a usar execv() em vez de
// reconstruir a Station in-process.
//
// reset() REPETIDO NA MESMA instancia, em contraste, FUNCIONA -- confirmado
// rodando (rl/tests/test_smoke.py chama reset() 4x seguidas no mesmo
// NativeSimulation): station->event(RESET_EVENT) restaura northM/eastM/
// altitudeM/fuelFraction bem proximos do valor inicial, com uma DERIVA
// numerica pequena (~1e-5 m em eastM, ~1e-6 em fuelFraction por reset) --
// resíduo de integracao do JSBSim entre uma chamada e outra, nao um erro de
// reset propriamente dito. Irrelevante para treino de RL (bem abaixo de
// qualquer ruido de acao/dinamica), mas documentado aqui para quem for
// depurar "por que o baseline nao bate byte a byte".
//------------------------------------------------------------------------------
class NativeSimulation
{
public:
   NativeSimulation(std::string scenarioPath, std::string playerName);
   ~NativeSimulation();

   NativeSimulation(const NativeSimulation&) = delete;
   NativeSimulation& operator=(const NativeSimulation&) = delete;

   // Devolve a observacao inicial (northM/eastM/... como estavam logo apos o
   // RESET_EVENT + o frame de assentamento).
   mixr::xrlbridge::Observation reset();

   // 1. publica 'cmd' em shared/xrlbridge (RLBridgeBehavior::genAction() o
   //    consome dentro do tcFrame() abaixo);
   // 2. station->tcFrame(dt); station->updateData(dt) -- mesma dupla chamada
   //    de app::runDeterministic(), ja provada deterministica;
   // 3. le de volta a Observation que RLBridgeBehavior::genAction() cacheou
   //    NESTE tcFrame(), e se o player (por nome) esta em modo CRASHED.
   //
   // Devolve (observacao, terminated).
   std::pair<mixr::xrlbridge::Observation, bool> step(const mixr::xrlbridge::Command& cmd);

private:
   std::string scenarioPath_;
   std::string playerName_;
   mixr::simulation::Station* station_{};
   bool built_{};
};

} // namespace rl
