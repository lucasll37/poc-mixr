#pragma once

#include "app/MetaObjectSnapshot.hpp"

#include <string>
#include <vector>

namespace mixr {
namespace models { class WorldModel; }
namespace xclock { class ClockStation; }
}

namespace app {

//------------------------------------------------------------------------------
// A "fotografia" de UMA entidade, pronta para desenhar -- generica por
// construcao: os campos obrigatorios vem todos de mixr::models::Player (a
// BASE, nao de AirVehicle), porque e o que um modelo QUALQUER (o flight de
// hoje, o missile, ou um modelo desconhecido futuro) garante ter. O bloco
// "hasFuel/hasGload/hasThrust" e o UNICO trecho especifico de aeronave --
// preenchido so quando um dynamic_cast<AirVehicle> funciona, e a UI (ver
// app/FleetPanel.cpp) esconde essas linhas quando o campo nao se aplica.
//
// captureState() roda na THREAD DE SIMULACAO (10 Hz, ver DashboardLoop.cpp),
// nunca na thread de desenho do FTXUI -- e por isso que ela devolve um valor
// por copia, em vez de expor ponteiros para dentro da Station: a thread de
// desenho so enxerga o ultimo estado publicado, sob um mutex (ver
// DashboardLoop.cpp).
//------------------------------------------------------------------------------
struct EntityState
{
   int id{};
   std::string name;

   // 'type:' do EDL se o modelo declarou; senao o nome da classe C++ mais
   // derivada via RTTI (typeid + demangle) -- funciona para QUALQUER .so
   // carregado, sem incluir nenhum header do modelo (ver DashboardState.cpp).
   std::string typeLabel;

   // Bitmask nativos de mixr::models::Player -- ja pensados pelo framework
   // para "que especie de player e este" e "de que lado", independente de
   // qualquer vocabulario proprio de modelo.
   unsigned int majorType{};
   unsigned int side{};

   // Ciclo de vida nativo (simulation::AbstractPlayer::Mode) -- ACTIVE,
   // CRASHED, DETONATED, DELETE_REQUEST... Mostrar isso e "informacao
   // instantanea" pedida: da pra ver uma entidade destruida sem procurar.
   int mode{};

   double northM{};
   double eastM{};
   double altitudeM{};
   double terrainElevM{};
   double altitudeAglM{};
   double headingDeg{};
   double rollDeg{};
   double pitchDeg{};
   double speedKts{};
   double machNum{};

   // Exclusivo de AirVehicle -- ver o comentario no topo da struct.
   bool hasFuel{};
   double fuelFrac{1.0};
   bool hasGload{};
   double gLoad{};
   bool hasThrust{};
   double thrustLb{};

   // shared/xboard::Readout -- ja agnostico por construcao (chave e int
   // playerId, valor e string livre que o MODELO escreve).
   std::string behaviorLabel{"--"};
   long decisions{};
   int threadTag{-1};

   bool hasAlert{};
   std::string alertSender;
   std::string alertContact;

   // shared/xtrack::nearestHostileTrack -- exclusivo de AirVehicle hoje (ver
   // o comentario no .cpp); so preenchido quando o cast funciona.
   bool hasTrack{};
   std::string trackName;
   double trackRangeNm{};
};

struct DashboardState
{
   std::string scenarioLabel;
   double wallSec{};
   double simSec{};
   double timeScale{1.0};
   bool paused{};
   int numTcThreads{};

   std::vector<EntityState> entities;

   // Amostra ao vivo dos contadores de instancia -- ver
   // app/MetaObjectSnapshot.hpp para o "porque" e o criterio de suspeita.
   std::vector<ClassStat> classStats;
};

// 'previousClassStats' e o 'classStats' do DashboardState anterior -- carrega
// o historico de janela deslizante entre amostras (ver
// app/MetaObjectSnapshot.hpp).
DashboardState captureState(mixr::models::WorldModel* worldModel,
                            double wallSec, double simSec,
                            const mixr::xclock::ClockStation* clockStation,
                            int numTcThreads, const std::string& scenarioLabel,
                            const std::vector<ClassStat>& previousClassStats);

} // namespace app
