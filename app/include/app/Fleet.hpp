#pragma once

#include <string>
#include <vector>

namespace mixr {
namespace models { class AirVehicle; class Player; class WorldModel; }
}

namespace app {

//------------------------------------------------------------------------------
// Os players que esta aplicacao observa.
//
// Uma unica questao: sair da arvore de objetos do cenario com ponteiros
// diretos para os players. Depois disso ninguem mais precisa varrer o
// PairStream do WorldModel -- status, dump e laco recebem a Fleet pronta.
//
// GENERICO sobre mixr::models::Player desde que o dump '-deterministic'
// deixou de ser cego para qualquer player que nao fosse AirVehicle (ex.: um
// Paratrooper, models/players/paratrooper) -- ANTES disso, um cenario de
// '-folder' sem nenhum AirVehicle produzia uma Fleet vazia e um dump
// '-deterministic' sem linha nenhuma, silenciosamente. Widening verificado
// como NO-OP para todo cenario existente: nenhum player nao-AirVehicle
// aparece no init de nenhuma poc hoje (falcon1..4/a4/c130/bandit1 sao todos
// AirVehicle). Campos exclusivos de AirVehicle (combustivel, Mach) sao lidos
// via dynamic_cast em app::applyCruiseThrottle()/app/DeterministicDump.cpp,
// mesmo padrao ja usado por app::DashboardState (ver DashboardState.cpp).
//------------------------------------------------------------------------------
using Fleet = std::vector<mixr::models::Player*>;

mixr::models::AirVehicle* findAircraft(mixr::models::WorldModel* wm, const std::string& name);

// Encerra o processo se algum nome nao existir no cenario: um player faltando
// e erro de configuracao, e seguir sem ele so adiaria a falha.
Fleet collectFleet(mixr::models::WorldModel* wm, const std::vector<std::string>& names);

//------------------------------------------------------------------------------
// TODOS os players do cenario, do tipo que forem -- sem lista de nomes, sem
// suposicao de que sao AirVehicle. E a fonte da aba "Frota"/"Mapa" do
// dashboard (app/DashboardState.cpp): chamada a cada amostragem (10 Hz), o
// que resolve de quebra entidades que nascem/somem em runtime (um missil
// lancado, um alvo destruido) -- ao contrario da Fleet nomeada acima, fixada
// na inicializacao.
//------------------------------------------------------------------------------
std::vector<mixr::models::Player*> discoverPlayers(mixr::models::WorldModel* wm);

//------------------------------------------------------------------------------
// Como discoverPlayers() acima, ja no formato que applyCruiseThrottle()/o
// dump deterministico esperam (Fleet == vector<Player*>, desde o widening
// documentado acima -- esta funcao e' hoje so um alias de discoverPlayers(),
// mantido por nome porque varios comentarios deste diretorio (Options.hpp,
// AdHocScenario.hpp, DashboardLoop.hpp) ja o citam). USO EXCLUSIVO das
// entradas sinteticas de '-folder <pasta>' (app/ScenarioFolder.hpp) quando
// ScenarioEntry::fleet vem vazio -- NUNCA do fallback de
// app::adHocScenario()/'-f', que continua devolvendo falconFleet()
// explicitamente. Essa funcao ja foi tentada como fallback
// GENERICO de '-f' e revertida: quebrava as fixtures de teste 'intruder'
// (tests/scenario/make_fixture.py), que tem um bandit1 LOCAL deliberadamente
// FORA da frota rastreada -- uma descoberta generica pegava esse bandit1
// tambem. Cenarios de '-folder' nao sao fixtures de teste, sao criacoes do
// proprio usuario -- essa razao nao se aplica a eles.
//------------------------------------------------------------------------------
Fleet discoverFleet(mixr::models::WorldModel* wm);

//------------------------------------------------------------------------------
// POTENCIA DE CRUZEIRO -- historicamente a correcao para o c310 (cujo
// autopilot fechava malha de RUMO e ALTITUDE, mas nao de VELOCIDADE, sem
// manete fixo a aeronave perdia velocidade e estolava). Desde a troca para
// o A-4, `models/players/A-4/data/jsbsim/aircraft/A4/a4ap.xml` tem um canal
// de autothrottle proprio (fecha a malha de velocidade de verdade, via
// `ap/airspeed_hold`/`ap/airspeed_setpoint`), entao esta chamada e so um
// empurrao inicial -- o autothrottle recalcula `fcs/throttle-cmd-norm` a
// cada frame e sobrescreve isto em seguida. Mantida por nao ser nociva
// (`setThrottles()` e do proprio framework) e por dar um chute inicial
// razoavel antes do primeiro ciclo do autothrottle.
//
// Desde o widening de Fleet, ignora (via dynamic_cast) qualquer player que
// nao seja AirVehicle -- um Paratrooper na Fleet simplesmente nao tem
// manete nenhum para fixar.
//------------------------------------------------------------------------------
void applyCruiseThrottle(const Fleet& fleet, double throttle);

} // namespace app
