#pragma once

#include "domain/FlightCommand.hpp"

#include "mixr/models/dynamics/DynamicsModel.hpp"

namespace bt_nodes {

// Fronteira domain -> MIXR: e a UNICA funcao que traduz um FlightCommand
// (regra de negocio pura) para a interface do dynamics model.
//
// Note que ela chama os metodos VIRTUAIS de models::DynamicsModel, nao os
// da nossa DroneDynamics: a arvore comanda "um dynamics model do MIXR", e
// so por isso a mesma arvore comandaria um RacModel sem alterar uma linha.
//
// Unidades (armadilha ja documentada no CLAUDE.md): setCommandedAltitude()
// espera METROS, setCommandedHeadingD() GRAUS, setCommandedVelocityKts()
// NOS. FlightCommand ja carrega a unidade no nome de cada campo.
inline void applyCommand(mixr::models::DynamicsModel* const dynamics,
                         const domain::FlightCommand& cmd)
{
   if (dynamics == nullptr) return;
   dynamics->setCommandedHeadingD(cmd.headingDeg);
   dynamics->setCommandedAltitude(cmd.altitudeM);
   dynamics->setCommandedVelocityKts(cmd.speedKts);
}

} // namespace bt_nodes
