#pragma once

#include "mixr/base/ubf/AbstractState.hpp"

#include "domain/DomePolicy.hpp"

#include <string>

namespace mixr {
namespace base { class Component; }

namespace models {
namespace xaaa {

//------------------------------------------------------------------------------
// Class: AaaState
//
// Description: A PERCEPCAO do UBF -- acha o alvo hostil mais proximo pelo
//              radar de aquisicao PROPRIO da antiaerea (via
//              xtrack::nearestHostileTrack, track manager nomeado
//              "aaaTrkMgr" no .edl) e le o domo de alcance
//              (SamVehicle::getMinLaunchRange()/getMaxLaunchRange(),
//              nativos) e se o cabide tem municao (StoresMgr::available()).
//
// Factory name: AaaState
//
// Slots: (nenhum)
//
// O agente UBF chama updateState(actor) uma vez por ciclo de decisao, ANTES
// de pedir a acao ao comportamento vencedor (ver
// mixr::base::ubf::Agent::controller()). O ator, aqui, e' a propria
// xnative::AaaSite (o Agent e' declarado DENTRO dos components: dela no
// .edl, entao Agent::initActor() -- que sobe so' um nivel, para o proprio
// container() -- ja resolve para a antiaerea, sem precisar de nenhuma
// subclasse de Agent/initActor() proprio, ao contrario de
// models/players/A-4/include/xnative/FlightAgentTC.hpp).
//
// ARMADILHA DO FRAMEWORK, ja medida em producao (mesmo comentario em
// models/players/A-4/include/ubf/FlightState.hpp): um Agent NAO propaga
// updateTC()/updateData() para os filhos, e 'state' e' filho do agente --
// este objeto NUNCA recebe o ciclo normal de componentes. Tudo o que ele
// precisa fazer tem que estar dentro de updateState().
//
// ARMADILHA CONFIRMADA (nao redescobrir, ver
// include/xnative/AaaSite.hpp): a checagem de municao usa
// StoresMgr::available() > 0 diretamente, NUNCA
// SamVehicle::isLauncherReady()/getNumberOfMissiles() -- os dois nativos
// contam via dynamic_cast<const Sam*>, e xmissile::GuidedMissile e' IRMA de
// Sam (nao filha), entao ficariam sempre falso/zero em silencio.
//------------------------------------------------------------------------------
class AaaState final : public base::ubf::AbstractState
{
   DECLARE_SUBCLASS(AaaState, base::ubf::AbstractState)

public:
   AaaState();

   void updateState(const base::Component* const actor) override;

   bool hasTarget() const                    { return hasTarget_; }
   const std::string& targetName() const     { return targetName_; }
   double targetRangeM() const               { return targetRangeM_; }
   bool weaponReady() const                  { return weaponReady_; }
   const domain::Dome& dome() const          { return dome_; }

private:
   bool hasTarget_{};
   std::string targetName_;
   double targetRangeM_{};
   bool weaponReady_{};
   domain::Dome dome_{};
};

} // namespace xaaa
} // namespace models
} // namespace mixr
