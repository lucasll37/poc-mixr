#ifndef __xnative_TrackQuery_H__
#define __xnative_TrackQuery_H__

#include <string>

namespace mixr {
namespace models { class AirVehicle; }

namespace xnative {

//------------------------------------------------------------------------------
// Consulta ao radar NATIVO: qual e o contato hostil mais proximo.
//
// Uma unica questao, e ela aparece em dois lugares muito diferentes -- na
// percepcao do UBF (ubf::FlightState) e no status/dump da aplicacao. Manter
// a consulta num so lugar garante que os dois digam a mesma coisa.
//
// O caminho e sempre o mesmo do framework:
//    AirVehicle -> OnboardComputer -> TrackManager("twsTrkMgr") -> Track
//
// DUAS REGRAS QUE NAO SAO DO SENSOR, E POR ISSO MORAM AQUI:
//
//   * o radar NATIVO nao filtra por lado ('playerOfInterestTypes' filtra por
//     TIPO de player, nao por side): a esquadrilha inteira entra na lista de
//     pistas. Separar amigo de inimigo e decisao tatica;
//   * desempate deterministico -- menor distancia e, em empate exato, menor
//     id de pista. Sem isso o resultado dependeria da ordem da lista, que
//     nao e garantida entre execucoes com numeros diferentes de threads.
//------------------------------------------------------------------------------
struct TrackInfo
{
   bool found{};
   std::string name;      // nome do player alvo, ou "trk<id>" se anonimo
   double rangeM{};
   double relBearingDeg{};

   // Vetor NED do contato RELATIVO ao ownship (metros).
   double relNorthM{};
   double relEastM{};
   double deltaAltM{};    // positivo = contato acima
};

TrackInfo nearestHostileTrack(const models::AirVehicle* air);

} // namespace xnative
} // namespace mixr

#endif
