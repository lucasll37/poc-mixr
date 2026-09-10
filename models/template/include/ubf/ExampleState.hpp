#pragma once

#include "mixr/base/ubf/AbstractState.hpp"

namespace mixr {
namespace base { class Component; }

namespace models {
namespace xtemplate {

//------------------------------------------------------------------------------
// Class: ExampleState
//
// Description: A PERCEPCAO do UBF -- le o ator e guarda uma leitura crua,
//              sem nenhum tipo do MIXR vazando para quem decide (ver
//              ubf::ExampleBehavior::genAction(), que so enxerga
//              getValue()).
//
// Factory name: ExampleState
//
// Slots: (nenhum)
//
// O agente UBF chama updateState(actor) uma vez por ciclo de decisao, ANTES
// de pedir a acao ao comportamento vencedor (ver
// mixr::base::ubf::Agent::updateData(), no fonte do MIXR). Note a
// assinatura: o ator chega como 'const Component*' -- percepcao LE, nao
// atua.
//
// ARMADILHA DO FRAMEWORK, ja medida em producao (ver o comentario
// equivalente em models/players/A-4/include/ubf/FlightState.hpp): um Agent NAO
// propaga updateTC()/updateData() para os filhos, e o 'state' e filho do
// agente -- este objeto NUNCA recebe o ciclo normal de componentes. Tudo o
// que ele precisa fazer tem que estar dentro de updateState().
//------------------------------------------------------------------------------
class ExampleState final : public base::ubf::AbstractState
{
   DECLARE_SUBCLASS(ExampleState, base::ubf::AbstractState)

public:
   ExampleState();

   void updateState(const base::Component* const actor) override;

   // A leitura crua deste frame -- troque por qualquer grandeza que o SEU
   // modelo precise decidir sobre (combustivel, alcance ao contato mais
   // proximo, carga de bateria...). Aqui e a altitude HAE do ator, so
   // porque 'Player::getAltitudeM()' existe em QUALQUER Player (nao so em
   // aeronaves) e da um exemplo que compila sem depender de uma classe de
   // veiculo especifica.
   double getValue() const   { return value; }

private:
   double value{};
};

} // namespace xtemplate
} // namespace models
} // namespace mixr
