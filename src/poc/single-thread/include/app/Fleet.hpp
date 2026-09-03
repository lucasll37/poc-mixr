#pragma once

#include <string>
#include <vector>

namespace mixr {
namespace models { class AirVehicle; class WorldModel; }
}

namespace app {

//------------------------------------------------------------------------------
// Os players que esta aplicacao observa.
//
// Uma unica questao: sair da arvore de objetos do cenario com ponteiros
// diretos para as aeronaves. Depois disso ninguem mais precisa varrer o
// PairStream do WorldModel -- status, dump e laco recebem a Fleet pronta.
//------------------------------------------------------------------------------
using Fleet = std::vector<mixr::models::AirVehicle*>;

mixr::models::AirVehicle* findAircraft(mixr::models::WorldModel* wm, const std::string& name);

// Encerra o processo se algum nome nao existir no cenario: um player faltando
// e erro de configuracao, e seguir sem ele so adiaria a falha.
Fleet collectFleet(mixr::models::WorldModel* wm, const std::vector<std::string>& names);

//------------------------------------------------------------------------------
// POTENCIA DE CRUZEIRO -- ver o comentario no scenario.epp: o autopilot do
// c310 fecha malha de RUMO e de ALTITUDE, mas nao de VELOCIDADE (o
// c310ap.xml apenas DECLARA ap/airspeed_hold e ap/throttle-cmd-norm, sem
// canal que os implemente). Sem manete, a aeronave perdia velocidade, o
// altitude hold empinava o nariz para compensar e acabava em perda de
// sustentacao -- observado rodando (40 s: 160 -> 80 kts, arfagem 15 graus).
//
// Correcao no nivel do CENARIO, nao uma lei de controle nossa:
// AirVehicle::setThrottles() e do proprio framework. A velocidade passa a
// ser RESULTADO (potencia fixa + arrasto), nao comando.
//------------------------------------------------------------------------------
void applyCruiseThrottle(const Fleet& fleet, double throttle);

} // namespace app
