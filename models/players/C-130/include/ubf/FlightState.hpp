#pragma once

#include "mixr/base/ubf/AbstractState.hpp"

#include "domain/WorldView.hpp"

namespace mixr {
namespace base { class Component; }

namespace models {
namespace xC_130 {

//------------------------------------------------------------------------------
// Class: FlightState
//
// Description: A PERCEPCAO do UBF -- le o ator (AirVehicle) e monta uma
//              visao do mundo em numeros crus, sem nenhum tipo do MIXR
//              vazando para quem decide.
//
// Factory name: C130FlightState
//
// Slots: (nenhum)
//
// O UBF chama updateState(actor) uma vez por ciclo, ANTES de pedir a acao ao
// comportamento (ver base/ubf/Agent.cpp). Note a assinatura: o ator chega
// como 'const Component*' -- percepcao le, nao atua.
//
// ARMADILHA DO FRAMEWORK, ja medida em producao (models/players/A-4): um Agent
// NAO propaga updateTC()/updateData() para os filhos, e o 'state' e filho do
// agente -- este objeto NUNCA recebe o ciclo normal de componentes. Tudo o
// que ele precisa fazer tem que estar dentro de updateState().
//------------------------------------------------------------------------------
class FlightState final : public base::ubf::AbstractState
{
   DECLARE_SUBCLASS(FlightState, base::ubf::AbstractState)

public:
   FlightState();

   using Snapshot = domain::WorldView;

   void updateState(const base::Component* const actor) override;

   const Snapshot& snapshot() const   { return snap; }

private:
   Snapshot snap;
};

} // namespace xC_130
} // namespace models
} // namespace mixr
