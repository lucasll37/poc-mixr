#ifndef __xnative_ubf_AltitudeSafetyBehavior_H__
#define __xnative_ubf_AltitudeSafetyBehavior_H__

#include "mixr/base/ubf/AbstractBehavior.hpp"

namespace mixr {
namespace base { class Distance; class Number; }

namespace xnative {

//------------------------------------------------------------------------------
// Class: AltitudeSafetyBehavior
//
// Description: Comportamento de seguranca -- se a aeronave desceu abaixo do
//              piso, gera uma acao de recuperacao; caso contrario, nao gera
//              acao nenhuma (nullptr).
//
// Factory name: AltitudeSafetyBehavior
//
// Slots:
//    minAltitude     <Distance>  ! Piso que dispara a recuperacao (default: 1500 m)
//    recoverAltitude <Distance>  ! Altitude alvo da recuperacao (default: 3500 m)
//    recoverSpeed    <Number>    ! Velocidade na recuperacao, kts (default: 400)
//    vote            <Number>    ! (herdado de AbstractBehavior)
//
// POR QUE ESTE COMPORTAMENTO EXISTE NA POC: para mostrar composicao do UBF
// que NAO passa pela arvore. Ele e irmao do BtBehavior dentro de um
// UbfArbiter nativo e tem voto MAIOR: quando a aeronave fura o piso, a
// acao dele vence a da arvore sem que a arvore precise saber que ele
// existe. Uma regra dura ("nunca abaixo de X") fica fora da politica
// tatica, que e onde ela deve estar.
//------------------------------------------------------------------------------
class AltitudeSafetyBehavior : public base::ubf::AbstractBehavior
{
   DECLARE_SUBCLASS(AltitudeSafetyBehavior, base::ubf::AbstractBehavior)

public:
   AltitudeSafetyBehavior();

   base::ubf::AbstractAction* genAction(const base::ubf::AbstractState* const state,
                                        const double dt) override;

private:
   double minAltitudeM{1500.0};
   double recoverAltitudeM{3500.0};
   double recoverSpeedKts{400.0};

   // slot table helper methods
   bool setSlotMinAltitude(const base::Distance* const);
   bool setSlotRecoverAltitude(const base::Distance* const);
   bool setSlotRecoverSpeed(const base::Number* const);
};

} // namespace xnative
} // namespace mixr

#endif
