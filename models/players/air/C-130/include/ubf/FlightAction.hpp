#pragma once

#include "mixr/base/ubf/AbstractAction.hpp"

#include "domain/FlightCommand.hpp"

#include <string>

namespace mixr {
namespace base { class Component; }

namespace models {
namespace xC_130 {

//------------------------------------------------------------------------------
// Class: FlightAction
//
// Description: A ATUACAO do UBF -- sabe se executar sobre o ator.
//
// Factory name: C130FlightAction
//
// E o UNICO ponto deste modelo que escreve nos subsistemas a partir da
// decisao: a arvore de comportamento nao toca em objeto MIXR nenhum, ela so
// preenche o comando que vai daqui para o Autopilot.
//
// Repare na assinatura de execute(): o ator chega como PARAMETRO. A acao
// nao guarda ponteiro para o ator, nao o conhece na construcao -- e o que
// desacopla decisao de atuacao no UBF.
//------------------------------------------------------------------------------
class FlightAction final : public base::ubf::AbstractAction
{
   DECLARE_SUBCLASS(FlightAction, base::ubf::AbstractAction)

public:
   FlightAction();

   bool execute(base::Component* actor) override;

   void setCommand(const domain::FlightCommand& cmd)   { command = cmd; }
   void setLabel(const std::string& text)              { label = text; }

private:
   domain::FlightCommand command{};
   std::string label{"?"};
};

} // namespace xC_130
} // namespace models
} // namespace mixr
