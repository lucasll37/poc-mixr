#pragma once

#include "mixr/base/ubf/AbstractState.hpp"

namespace mixr {
namespace base { class Component; }

namespace models {
namespace xparatrooper {

//------------------------------------------------------------------------------
// Class: ParatrooperState
//
// Description: A PERCEPCAO do UBF -- le o ator (o proprio
//              xnative::Paratrooper) e guarda a AGL crua e a validade do
//              terreno, sem nenhum tipo do MIXR vazando para quem decide
//              (ver ubf::ParatrooperBtBehavior::genAction()).
//
// Factory name: ParatrooperState
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
//------------------------------------------------------------------------------
class ParatrooperState final : public base::ubf::AbstractState
{
   DECLARE_SUBCLASS(ParatrooperState, base::ubf::AbstractState)

public:
   ParatrooperState();

   void updateState(const base::Component* const actor) override;

   // NAO chamar de 'isValid()' -- mixr::base::Object ja declara essa
   // virtual (usada pelo parser de EDL logo apos construir o objeto, antes
   // de updateState() rodar uma vez sequer); um metodo com a MESMA
   // assinatura aqui a sobrescreveria (sem precisar de 'override' para
   // colidir) e faria TODO '( ParatrooperState )' no .edl falhar com
   // "invalid object" -- ja aconteceu, medido rodando 'edlcheck'.
   bool hasReading() const        { return valid; }
   double getAglM() const         { return aglM; }
   double getAltitudeM() const    { return altitudeM; }
   bool isTerrainValid() const    { return terrainValid; }

private:
   bool valid{false};
   double aglM{};
   double altitudeM{};
   bool terrainValid{false};
};

} // namespace xparatrooper
} // namespace models
} // namespace mixr
