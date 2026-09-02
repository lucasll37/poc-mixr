#pragma once

#include "app/BehaviorTreeView.hpp"

#include <string>

namespace mixr {
namespace simulation { class Station; }
namespace models { class WorldModel; }
namespace xclock { class ClockStation; }
namespace xtacview { class TacviewOutput; }
}

namespace app {

// O que fazer depois que o laco termina (main.cpp decide com isto: sair,
// reexecutar com o MESMO cenario, ou reexecutar sem '-scenario' -- ver
// app/Respawn.hpp para o "porque" de ser sempre um reexec, nunca uma
// segunda Station no mesmo processo).
enum class DashboardExit { Quit, Restart, ChangeScenario };

//------------------------------------------------------------------------------
// O laco de tempo real desta poc -- substitui app/RealTimeRun.{hpp,cpp} das
// outras pocs. Mesma cadeia de trabalho por frame (station->updateData(dt),
// varredura de radar pro Tacview, o mesmo espacamento de 10 Hz por relogio
// de parede), so que a impressao de status vira um dashboard FTXUI de TRES
// abas -- Frota (lista rolavel + detalhe), Mapa (navegavel) e Memoria
// (contadores de instancia ao vivo, ver app/MetaObjectSnapshot.hpp) -- e as
// teclas de controle de tempo chamam ClockStation DIRETO (nao usa
// shared/xclock::TimeControls/ConsoleKeyboard -- o FTXUI ja e dono do
// terminal, ver o cabecalho de DashboardLoop.cpp).
//
// SEM 'Fleet' no parametro: a descoberta de entidades (para exibir E para
// empurrar a varredura de radar pro Tacview) e generica, direto de
// 'worldModel' (ver app::discoverPlayers em app/Fleet.hpp) -- a Fleet
// nomeada continua existindo em main.cpp, so para o fixup de
// applyCruiseThrottle, uma questao de SETUP anterior a este laco.
//
// Bloqueia ate o usuario sair/reiniciar/trocar de cenario.
//------------------------------------------------------------------------------
DashboardExit runDashboard(mixr::simulation::Station* station,
                           mixr::models::WorldModel* worldModel,
                           mixr::xclock::ClockStation* clockStation,
                           mixr::xtacview::TacviewOutput* tacviewOutput,
                           int numTcThreads, const std::string& scenarioLabel,
                           const BtNode& behaviorTree);

} // namespace app
