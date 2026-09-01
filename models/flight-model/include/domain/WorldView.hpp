#pragma once

#include <string>

namespace domain {

//------------------------------------------------------------------------------
// WorldView -- a visao do mundo de UM frame, em numeros crus.
//
// Era 'FlightState::Snapshot', aninhada dentro da classe de percepcao do UBF.
// O conteudo nunca teve tipo nenhum do MIXR; o que prendia a estrutura ao
// framework era so o lugar onde ela morava -- dentro de uma classe que herda
// de base::ubf::AbstractState.
//
// Morando aqui, em domain/, quem CONSOME a percepcao (os nos da arvore, as
// politicas) compila sem o MIXR. A percepcao continua sendo produzida pela
// xnative::FlightState, que mantem o alias 'FlightState::Snapshot' para esta
// estrutura -- nenhum call site precisou mudar.
//
// E de proposito que isto e um struct de dados, e nao uma classe com
// comportamento: a fronteira entre "ler o mundo" e "decidir" e justamente
// nao deixar tipo de framework atravessar.
//------------------------------------------------------------------------------
struct WorldView
{
   bool valid{};

   // proprio
   double northM{};
   double eastM{};
   double altitudeM{};
   double headingDeg{};
   double speedKts{};
   double rollDeg{};
   double pitchDeg{};
   double fuelFraction{1.0};
   double mach{};
   double gLoad{1.0};
   double alphaDeg{};

   // referencia de solo (banco de elevacao do WorldModel, consultado pelo
   // Player::updateElevation() nativo)
   bool terrainValid{};
   double terrainElevM{};    // elevacao do terreno sob a aeronave (MSL)
   double altitudeAglM{};    // altitudeM - terrainElevM

   // contato do sensor proprio
   bool hasContact{};
   std::string contactName;
   double contactRangeM{};
   double contactRelBearingDeg{};
   double contactDeltaAltM{};
   double contactNorthM{};   // posicao absoluta (para transmitir no alerta)
   double contactEastM{};
   double contactAltitudeM{};

   // alerta recebido de OUTRO aviao (ver xnative::AlertDatalink)
   bool hasAlert{};
   std::string alertSender;
   std::string alertContactName;
   double alertNorthM{};
   double alertEastM{};
   double alertAltitudeM{};
   double alertRangeM{};
};

} // namespace domain
