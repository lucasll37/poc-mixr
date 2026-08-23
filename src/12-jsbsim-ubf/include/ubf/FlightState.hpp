#ifndef __xair_ubf_FlightState_H__
#define __xair_ubf_FlightState_H__

#include "mixr/base/ubf/AbstractState.hpp"

#include <string>

namespace mixr {
namespace xair {

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
   struct Snapshot
   {
      bool valid{};

      // proprio
      double northM{};
      double eastM{};
      double altitudeM{};
      double headingDeg{};
      double speedKts{};
      double rollDeg{};
      double pitchDeg{};
      double fuelFraction{1.0};
      double mach{};
      double gLoad{1.0};
      double alphaDeg{};

      // contato do sensor proprio
      bool hasContact{};
      std::string contactName;
      double contactRangeM{};
      double contactRelBearingDeg{};
      double contactDeltaAltM{};
      double contactNorthM{};   // posicao absoluta (para transmitir no alerta)
      double contactEastM{};
      double contactAltitudeM{};

      // alerta recebido de OUTRO aviao (ver xair::AlertRadio)
      bool hasAlert{};
      std::string alertSender;
      std::string alertContactName;
      double alertNorthM{};
      double alertEastM{};
      double alertAltitudeM{};
      double alertRangeM{};
   };

   void updateState(const base::Component* const actor) override;

   const Snapshot& snapshot() const   { return snap; }

private:
   Snapshot snap;
};

} // namespace xair
} // namespace mixr

#endif
