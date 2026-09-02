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
