#include "app/DeterministicDump.hpp"

#include "xboard/Board.hpp"
#include "xtrack/TrackQuery.hpp"

#include "mixr/models/player/Player.hpp"
#include "mixr/models/player/air/AirVehicle.hpp"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace app {

void printDeterministicDump(const Fleet& fleet, const long frame)
{
   std::ostringstream oss;
   oss << std::fixed << std::setprecision(9);

   for (const auto player : fleet) {
      if (player == nullptr) continue;

      const mixr::base::Vec3d& pos{player->getPosition()};

      // Tudo o que vem do MODELO chega por aqui, e so por aqui. O modelo mora
      // num .so carregado com dlopen, entao este arquivo nao pode incluir
      // nenhum header dele -- nem para um dynamic_cast, porque o typeinfo do
      // plugin nao e visivel. Ver libs/xboard/Board.hpp.
      //
      // E por isso que este arquivo e byte-identico entre pocs: o 'dec=' e
      // o 'bt=' vem sempre do MESMO lugar (o xboard), nunca de um tipo
      // concreto de Player.
      const mixr::xboard::Readout board{mixr::xboard::get(player->getID())};

      // 'mach=', 'fuel=' e a busca de pista (radar/track manager) sao
      // conceito AERODINAMICO, exclusivo de AirVehicle -- 0.0/nenhuma pista
      // para qualquer outro tipo de player (ex.: um Paratrooper,
      // models/players/paratrooper), mesmo padrao ja usado por
      // app::DashboardState (ver DashboardState.cpp) para os campos
      // opcionais do dashboard.
      double mach{};
      double fuelWt{};
      mixr::xtrack::TrackInfo track{};
      if (const auto* const air = dynamic_cast<const mixr::models::AirVehicle*>(player)) {
         mach = air->getMach();
         fuelWt = air->getFuelWt();
         track = mixr::xtrack::nearestHostileTrack(air);
      }

      oss << "frame=" << frame
          << " player=" << (player->getName() != nullptr ? player->getName()->getString() : "?")
          << " n=" << pos[mixr::models::Player::INORTH]
          << " e=" << pos[mixr::models::Player::IEAST]
          << " alt=" << player->getAltitudeM()
          // Elevacao e AGL sao estado da simulacao (consulta ao banco de
          // elevacao, imutavel depois de carregado) -- entram no dump pelo
          // mesmo criterio do resto: e por eles que se prova que a consulta
          // ao terreno da o MESMO resultado com 1, 2 e 4 threads T/C.
          << " elev=" << player->getTerrainElevationM()
          << " agl=" << player->getAltitudeAglM()
          << " hdg=" << player->getHeadingD()
          << " roll=" << player->getRollD()
          << " pitch=" << player->getPitchD()
          << " spd=" << player->getTotalVelocity()
          << " mach=" << mach
          << " fuel=" << fuelWt
          << " bt=" << board.label
          << " track=" << (track.found ? track.name : std::string("none"))
          << " trackRange=" << track.rangeM
          << " alert=" << (board.alertValid ? board.alertSender : std::string("none"))
          << " sent=" << board.sent
          << " recv=" << board.received
          // Contagem de decisoes: entra no dump de proposito. Se a decisao
          // esta mesmo amarrada ao frame, este numero tem que avancar na
          // MESMA taxa que 'frame' entre dois dumps, com 1, 2 ou 4 threads.
          //
          // Conta no ponto da ATUACAO (ubf::FlightAction::execute) -- antes o
          // agente contava em si mesmo, o que dava uma decisao a mais na
          // inicializacao. O que se afirma sempre foi o avanco, nao o valor
          // absoluto (ver tests/determinism).
          << " dec=" << board.decisions
          << std::endl;
   }

   std::cout << oss.str();
}

} // namespace app
