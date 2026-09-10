#pragma once

#include "mixr/base/ubf/AbstractState.hpp"

namespace mixr {
namespace base { class Component; }

namespace models {
namespace xNavstar_3 {

//------------------------------------------------------------------------------
// Class: Navstar3State
//
// Description: A PERCEPCAO do UBF -- confirma que o ator e um Player valido
//              e guarda a altitude HAE dele, so como diagnostico (a posicao
//              de verdade e CALCULADA pelo propagador orbital em
//              ubf::Navstar3BtBehavior, nao lida do Player -- e o Player
//              que a segue, nao o contrario).
//
// Factory name: Navstar3State
//
// Slots: (nenhum)
//
// O agente UBF chama updateState(actor) uma vez por ciclo de decisao, ANTES
// de pedir a acao ao comportamento vencedor. Note a assinatura: o ator chega
// como 'const Component*' -- percepcao LE, nao atua.
//
// ARMADILHA DO FRAMEWORK, ja documentada em varios modelos deste
// repositorio: um Agent NAO propaga updateTC()/updateData() para os filhos,
// e o 'state' e filho do agente -- este objeto NUNCA recebe o ciclo normal
// de componentes. Tudo o que ele precisa fazer tem que estar dentro de
// updateState().
//
// NAO chamar de 'isValid()' -- mixr::base::Object ja declara essa virtual
// (usada pelo parser de EDL logo apos construir o objeto); um metodo com a
// MESMA assinatura aqui a sobrescreveria em silencio e faria todo
// ( Navstar3State ) do .edl falhar com "invalid object" (achado ja medido
// no modelo paratrooper deste repositorio -- nao redescobrir).
//------------------------------------------------------------------------------
class Navstar3State final : public base::ubf::AbstractState
{
   DECLARE_SUBCLASS(Navstar3State, base::ubf::AbstractState)

public:
   Navstar3State();

   void updateState(const base::Component* const actor) override;

   bool hasReading() const     { return valid; }
   double getAltitudeM() const { return altitudeM; }

private:
   bool valid{false};
   double altitudeM{};
};

} // namespace xNavstar_3
} // namespace models
} // namespace mixr
