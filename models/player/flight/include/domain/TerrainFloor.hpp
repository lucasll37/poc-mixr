#pragma once

namespace domain {

// A referencia de solo que a decisao enxerga. Como todo o resto de domain/,
// nao conhece MIXR nem terrain::Terrain: quem traduz Player -> GroundReference
// e a fronteira (ubf::FlightState, depois ubf::BtBehavior).
//
// Unidade explicita no nome do campo, como no FlightCommand.
struct GroundReference
{
   bool   valid{};          // ha banco de elevacao respondendo por este ponto
   double elevationM{};     // elevacao do terreno sob a aeronave (MSL, metros)
};

//------------------------------------------------------------------------------
// O piso de altitude que uma decisao pode comandar.
//
// Duas camadas, e a de baixo e a que importa:
//
//   terreno + folga   quando ha dado de elevacao
//   piso absoluto     SEMPRE, como minimo
//
// POR QUE o piso absoluto continua existindo depois de haver terreno. O
// Player::updateElevation() do framework IGNORA o retorno de getElevation()
// (Player.cpp:3205-3206): uma aeronave fora da celula do tile recebe
// elevacao 0.0 com o flag de validade LIGADO. Nao existe, portanto, uma
// forma honesta de perguntar ao Player "voce esta coberto?" -- e um piso
// derivado so do terreno cairia para o nivel do mar exatamente no caso em
// que se sabe menos sobre o mundo. O piso absoluto e a rede para isso, e e
// por ele que o comportamento degrada para o de antes do terreno quando o
// banco falta.
//------------------------------------------------------------------------------
double terrainFloorM(const GroundReference& ground, double clearanceM,
                     double absoluteFloorM);

// Aplica o piso a uma altitude ja comandada. Nunca abaixa nada -- so levanta.
double clampToTerrain(double commandedAltM, const GroundReference& ground,
                      double clearanceM, double absoluteFloorM);

} // namespace domain
