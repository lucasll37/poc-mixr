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
// diretos para as aeronaves. Depois disso ninguem mais precisa varrer o
// PairStream do WorldModel -- status, dump e laco recebem a Fleet pronta.
//
// SO para o fixup de app/applyCruiseThrottle (uma questao de SETUP: o c310
// precisa de manete fixo -- ver o comentario abaixo). A EXIBICAO no
// dashboard nao usa mais isto -- ver discoverPlayers().
//------------------------------------------------------------------------------
using Fleet = std::vector<mixr::models::AirVehicle*>;

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
// POTENCIA DE CRUZEIRO -- historicamente a correcao para o c310 (cujo
// autopilot fechava malha de RUMO e ALTITUDE, mas nao de VELOCIDADE, sem
// manete fixo a aeronave perdia velocidade e estolava). Desde a troca para
// o A-4, `models/player/A4/data/jsbsim/aircraft/A4/a4ap.xml` tem um canal
// de autothrottle proprio (fecha a malha de velocidade de verdade, via
// `ap/airspeed_hold`/`ap/airspeed_setpoint`), entao esta chamada e so um
// empurrao inicial -- o autothrottle recalcula `fcs/throttle-cmd-norm` a
// cada frame e sobrescreve isto em seguida. Mantida por nao ser nociva
// (`setThrottles()` e do proprio framework) e por dar um chute inicial
// razoavel antes do primeiro ciclo do autothrottle.
//------------------------------------------------------------------------------
void applyCruiseThrottle(const Fleet& fleet, double throttle);

} // namespace app
