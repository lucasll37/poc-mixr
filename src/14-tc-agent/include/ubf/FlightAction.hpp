#ifndef __xnative_ubf_FlightAction_H__
#define __xnative_ubf_FlightAction_H__

#include "mixr/base/ubf/AbstractAction.hpp"

#include "domain/FlightCommand.hpp"

#include <string>

namespace mixr {
namespace xnative {

//------------------------------------------------------------------------------
// Class: FlightAction
//
// Description: A ATUACAO do UBF -- sabe se executar sobre o ator.
//
// Factory name: FlightAction
//
// E o UNICO ponto desta poc que escreve nos subsistemas a partir da
// decisao: a arvore de comportamento nao toca em objeto MIXR nenhum, ela
// so preenche o comando que vai daqui para o FlightDirector (e, quando for
// o caso, o pedido de transmissao para o AlertRadio).
//
// Repare na assinatura de execute(): o ator chega como PARAMETRO. A acao
// nao guarda ponteiro para o ator, nao o conhece na construcao e pode ser
// gerada por um comportamento que nem sabe de quem e a aeronave -- e o que
// desacopla decisao de atuacao no UBF.
//------------------------------------------------------------------------------
class FlightAction : public base::ubf::AbstractAction
{
   DECLARE_SUBCLASS(FlightAction, base::ubf::AbstractAction)

public:
   FlightAction();

   bool execute(base::Component* actor) override;

   void setCommand(const domain::FlightCommand& cmd)   { command = cmd; }
   void setLabel(const std::string& text)              { label = text; }

   // Pedido de transmissao do alerta tatico (a transmissao em si acontece
   // na fase 1 do frame seguinte -- ver xnative::AlertRadio).
   void setAlertBroadcast(const std::string& contactName,
                          const double northM, const double eastM,
                          const double altitudeM, const double rangeM);

private:
   domain::FlightCommand command{};
   std::string label{"?"};

   bool broadcast{};
   std::string alertContactName;
   double alertNorthM{};
   double alertEastM{};
   double alertAltitudeM{};
   double alertRangeM{};
};

} // namespace xnative
} // namespace mixr

#endif
