#pragma once

#include "mixr/base/ubf/AbstractAction.hpp"

#include "mixr/base/osg/Vec3d"

#include <string>

namespace mixr {
namespace base { class Component; }

namespace models {
namespace xNavstar_3 {

//------------------------------------------------------------------------------
// Class: Navstar3Action
//
// Description: A ATUACAO do UBF -- move o Player para a posicao ECEF que
//              ubf::Navstar3BtBehavior calculou (via
//              Player::setGeocPosition(pos, slaved=true)/setGeocVelocity()) e
//              escreve o rotulo sol/sombra no xboard.
//
// Factory name: Navstar3Action
//
// Slots: (nenhum -- a posicao/velocidade/rotulo chegam prontos do
//        comportamento que a criou, ver
//        ubf::Navstar3BtBehavior::genAction())
//
// 'slaved=true' e' o que desliga o integrador cinematico NATIVO de
// Player::positionUpdate() para este player (marca posSlaved/altSlaved) --
// sem isso, o integrador nativo (que so anda em linha reta na velocidade
// corrente) disputaria a posicao com o calculo orbital desta classe a cada
// frame. Ver docs/ARCHITECTURE.md para o porque de NAO existir
// DynamicsModel nenhum anexado a este player.
//
// O que execute() NAO PODE deixar de fazer, em QUALQUER modelo, e escrever
// no xboard -- ver models/players/template/docs/CONTRATO.md secao 3. E a
// UNICA obrigacao de um modelo que falha em SILENCIO: sem ela, o host sobe,
// o cenario parseia, tudo passa, e a tela de status/o dump '-deterministic'
// mostram 'bt=--'/'dec=0' para sempre, sem erro nenhum em lugar nenhum.
//------------------------------------------------------------------------------
class Navstar3Action final : public base::ubf::AbstractAction
{
   DECLARE_SUBCLASS(Navstar3Action, base::ubf::AbstractAction)

public:
   Navstar3Action();
   Navstar3Action(const std::string& label, const base::Vec3d& ecefPosM, const base::Vec3d& ecefVelMps);

   bool execute(base::Component* actor) override;

private:
   std::string label{"SUNLIT"};
   base::Vec3d ecefPosM{};
   base::Vec3d ecefVelMps{};
};

} // namespace xNavstar_3
} // namespace models
} // namespace mixr
