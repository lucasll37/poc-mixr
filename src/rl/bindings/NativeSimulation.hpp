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
// mesmo limite em libs/xrlbridge/RLBridge.hpp e
// models/players/air/A-4/include/ubf/RLBridgeBehavior.hpp).
//
// 'playerName' tem de ser o mesmo player configurado com
// ( RLBridgeBehavior ) no .edl (default: falcon1): como libs/xrlbridge nao
// tem chave por player id, o Command/Observation trocados por step()/
// reset() sao sempre os do player que o .edl escolheu, nao os de
// 'playerName'. Um valor diferente afeta silenciosamente so a checagem de
// 'terminated'. reset() falha alto (std::runtime_error) se o player nao
// existir no cenario; nao e possivel validar daqui que e o mesmo player com
// RLBridgeBehavior, tipo que mora no plugin do modelo
// (tests/guard/check_core_opaco.sh).
//
// So pode existir uma Station por processo: libs/xplugin sela o registro de
// plugins depois do primeiro edl_parser() (mixr::xplugin::seal(), dentro de
// buildStation()); um segundo NativeSimulation no mesmo processo, ao chamar
// reset() pela primeira vez, cai em buildStation() -> edl_parser() de novo
// e o registro recusa com "loadModule(...) depois do parse". Trocar de
// cenario ou reiniciar do zero exige um processo novo (multiprocessing ou
// reexec) -- mesmo raciocinio que leva app/Respawn.hpp a usar execv() em
// vez de reconstruir a Station in-process.
//
// reset() repetido na mesma instancia funciona: station->event(RESET_EVENT)
// restaura northM/eastM/altitudeM/fuelFraction proximos do valor inicial,
// com deriva numerica pequena (~1e-5 m em eastM, ~1e-6 em fuelFraction por
// reset) -- resíduo de integracao do JSBSim entre chamadas, nao um erro de
// reset. Irrelevante para treino de RL; documentado para quem investigar
// por que o baseline nao bate byte a byte.
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

   // 1. publica 'cmd' em libs/xrlbridge (RLBridgeBehavior::genAction() o
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
