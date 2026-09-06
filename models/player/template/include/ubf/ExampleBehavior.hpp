#pragma once

#include "mixr/base/ubf/AbstractBehavior.hpp"

#include "domain/ExampleThreshold.hpp"

namespace mixr {
namespace base { class Distance; }

namespace models {
namespace xtemplate {

//------------------------------------------------------------------------------
// Class: ExampleBehavior
//
// Description: A DECISAO do UBF -- aplica UMA regra pura de domain/ sobre a
//              percepcao e devolve uma acao rotulada.
//
// Factory name: ExampleBehavior
//
// Slots:
//    onValue  <Distance> ! Engaja quando a leitura sobe ATE ou ALEM disto (default: 1000 m)
//    offValue <Distance> ! So desengaja quando a leitura cai ABAIXO disto (default: 800 m)
//    vote     <Number>   ! (herdado de AbstractBehavior)
//
// PADRAO: percepcao (ExampleState) entra, decisao aplica UMA regra pura de
// domain/ (ExampleThreshold), acao (ExampleAction) sai rotulada e escreve
// no xboard. E o mesmo desenho de
// models/player/A4/include/ubf/BtBehavior.hpp, so sem uma arvore de
// comportamento por dentro -- aqui a "arvore" e uma regra so. Quando o SEU
// modelo crescer alem de uma decisao unica, e aqui que a
// BehaviorTree.CPP entraria (ver docs/ARCHITECTURE.md, secao "Quando isto
// nao bastar mais", e models/player/A4 como referencia completa).
//
// 'engaged' e ESTADO PROPRIO deste comportamento, persistido entre
// chamadas -- e o que faz o Schmitt trigger funcionar (sem lembrar o
// estado anterior, toda decisao reavaliaria so contra 'onValue', e a
// histerese nao existiria). Cada player do cenario tem sua PROPRIA
// instancia de ( ExampleBehavior ... ), entao este campo nunca e
// compartilhado entre players -- mesmo raciocinio de
// domain::PatrolPlan/RtbPlan em models/player/A4.
//------------------------------------------------------------------------------
class ExampleBehavior final : public base::ubf::AbstractBehavior
{
   DECLARE_SUBCLASS(ExampleBehavior, base::ubf::AbstractBehavior)

public:
   ExampleBehavior();

   base::ubf::AbstractAction* genAction(const base::ubf::AbstractState* const state,
                                        const double dt) override;

private:
   bool setSlotOnValue(const base::Distance* const);
   bool setSlotOffValue(const base::Distance* const);

   domain::ExampleThreshold rule{};
   bool engaged{false};
};

} // namespace xtemplate
} // namespace models
} // namespace mixr
