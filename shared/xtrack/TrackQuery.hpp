#ifndef __xtrack_TrackQuery_H__
#define __xtrack_TrackQuery_H__

#include "mixr/models/player/Player.hpp"

#include <string>
#include <vector>

namespace mixr {
namespace models { class AirVehicle; }

namespace xtrack {

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

//------------------------------------------------------------------------------
// A REGRA de selecao, separada da TRAVESSIA que a alimenta (AirVehicle ->
// OnboardComputer -> TrackManager -> Track[]) -- as duas regras do
// comentario acima ("nao sao do sensor") em forma testavel sem MIXR ao
// vivo: nenhum ponteiro Track/Player aqui, so os tres campos que a regra
// de fato le.
//------------------------------------------------------------------------------
struct TrackCandidate
{
   int trackId{};
   double rangeM{};

   // Fiel a regra original ('trk->getTarget()' pode ser nullptr -- pista
   // ainda nao correlacionada a um Player): o filtro de lado NUNCA se
   // aplica quando nao ha alvo resolvido. 'side' so e CONSULTADO quando
   // hasResolvedTarget==true.
   bool hasResolvedTarget{true};
   models::Player::Side side{models::Player::GRAY};
};

// Filtra candidatos do MESMO lado que 'ownSide' (so quando
// hasResolvedTarget==true -- ver o comentario de TrackCandidate) e
// escolhe o de menor 'rangeM'; em empate EXATO, o de menor 'trackId' --
// desempate deterministico, independente da ordem de 'candidates' (a
// lista de pistas do TrackManager nao tem ordem garantida entre execucoes
// com numeros diferentes de threads T/C).
//
// Devolve o INDICE do vencedor em 'candidates', ou -1 se nenhum candidato
// sobreviver ao filtro (lista vazia ou so-amigos).
int selectNearestHostileIndex(const std::vector<TrackCandidate>& candidates,
                              models::Player::Side ownSide);

} // namespace xtrack
} // namespace mixr

#endif
