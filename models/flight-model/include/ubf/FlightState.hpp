#ifndef __xnative_ubf_FlightState_H__
#define __xnative_ubf_FlightState_H__

#include "mixr/base/ubf/AbstractState.hpp"

#include "domain/WorldView.hpp"

#include <string>

namespace mixr {
namespace xnative {

//------------------------------------------------------------------------------
// Class: FlightState
//
// Description: A PERCEPCAO do UBF -- le o ator (Airplane) e monta uma visao
//              do mundo em numeros crus, sem nenhum tipo do MIXR vazando
//              para quem decide.
//
// Factory name: FlightState
//
// Slots: (nenhum)
//
// O UBF chama updateState(actor) uma vez por ciclo, ANTES de pedir a acao
// ao comportamento (ver base/ubf/Agent.cpp). Note a assinatura: o ator
// chega como 'const Component*' -- percepcao le, nao atua. Por isso o
// Airplane tem getters const para os subsistemas.
//
// ARMADILHA DO FRAMEWORK: um Agent NAO propaga updateTC()/updateData() aos
// filhos, e o 'state' e filho do agente. Ou seja, este objeto NUNCA recebe
// o ciclo normal de componentes -- se a percepcao dependesse disso, nunca
// rodaria. Tudo o que ele precisa fazer tem que estar em updateState().
//------------------------------------------------------------------------------
class FlightState : public base::ubf::AbstractState
{
   DECLARE_SUBCLASS(FlightState, base::ubf::AbstractState)

public:
   FlightState();

   // Visao do mundo daquele frame -- numeros crus, sem tipos do MIXR.
   //
   // A estrutura mora em domain/WorldView.hpp: quem CONSOME a percepcao (os
   // nos da arvore, as politicas) precisa dela sem arrastar o MIXR junto.
   // O alias mantem 'FlightState::Snapshot' valido em todos os call sites.
   using Snapshot = domain::WorldView;

   void updateState(const base::Component* const actor) override;

   const Snapshot& snapshot() const   { return snap; }

private:
   Snapshot snap;
};

} // namespace xnative
} // namespace mixr

#endif
