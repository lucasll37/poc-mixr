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

   for (const auto air : fleet) {
      if (air == nullptr) continue;

      const mixr::base::Vec3d& pos{air->getPosition()};
      const mixr::xtrack::TrackInfo track{mixr::xtrack::nearestHostileTrack(air)};

      // Tudo o que vem do MODELO chega por aqui, e so por aqui. O modelo mora
      // num .so carregado com dlopen, entao este arquivo nao pode incluir
      // nenhum header dele -- nem para um dynamic_cast, porque o typeinfo do
      // plugin nao e visivel. Ver shared/xboard/Board.hpp.
      //
      // E por isso que este arquivo e byte-identico nas duas pocs: o 'dec=' e
      // o 'bt=' deixaram de vir de lugares diferentes em cada uma.
      const mixr::xboard::Readout board{mixr::xboard::get(air->getID())};

      oss << "frame=" << frame
          << " player=" << (air->getName() != nullptr ? air->getName()->getString() : "?")
          << " n=" << pos[mixr::models::Player::INORTH]
          << " e=" << pos[mixr::models::Player::IEAST]
          << " alt=" << air->getAltitudeM()
          // Elevacao e AGL sao estado da simulacao (consulta ao banco de
          // elevacao, imutavel depois de carregado) -- entram no dump pelo
          // mesmo criterio do resto: e por eles que se prova que a consulta
          // ao terreno da o MESMO resultado com 1, 2 e 4 threads T/C.
          << " elev=" << air->getTerrainElevationM()
          << " agl=" << air->getAltitudeAglM()
          << " hdg=" << air->getHeadingD()
          << " roll=" << air->getRollD()
          << " pitch=" << air->getPitchD()
          << " spd=" << air->getTotalVelocity()
          << " mach=" << air->getMach()
          << " fuel=" << air->getFuelWt()
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
          // Conta no ponto da ATUACAO (ubf::FlightAction::execute), nas duas
          // pocs -- antes a multi-thread contava no proprio agente, o que dava
          // uma decisao a mais na inicializacao. O que se afirma sempre foi o
          // avanco, nao o valor absoluto (ver tests/determinism).
          << " dec=" << board.decisions
          << std::endl;
   }

   std::cout << oss.str();
}

} // namespace app
