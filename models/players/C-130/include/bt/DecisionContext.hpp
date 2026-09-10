#pragma once

#include "bt/NodeContext.hpp"
#include "domain/WorldView.hpp"

namespace mixr {
namespace models {
namespace xC_130 {
namespace bt {

//------------------------------------------------------------------------------
// DecisionContext -- o que um no da arvore precisa do comportamento que o
// hospeda, e NADA ALEM DISSO.
//
// Interface MINIMA (3 metodos) comparada aos 9 getters de
// models/players/A-4/include/bt/DecisionContext.hpp: este modelo nao tem
// patrulha/RTB/ameaca/piso anti-CFIT -- so navega. ubf::BtBehavior a
// implementa sem escrever metodo novo, as assinaturas ja sao os membros
// dela.
//
// POR QUE UMA INTERFACE, e nao um ponteiro para ubf::BtBehavior direto: os
// nos incluiriam "ubf/BtBehavior.hpp", e com ele viria o MIXR inteiro
// (AbstractBehavior -> Component -> Object). Com a interface, bt/**.cpp
// compila contra BehaviorTree.CPP + domain/ apenas, e tests/tree/ carrega a
// arvore de producao contra um contexto falso, sem simulacao nenhuma.
//------------------------------------------------------------------------------
class DecisionContext
{
public:
   virtual ~DecisionContext() = default;

   // percepcao do frame (a leitura crua que ubf::FlightState produziu)
   virtual const domain::WorldView& snapshot() const = 0;

   // o que a arvore preenche neste tick
   virtual FlightDecision& decision() = 0;

   // dt do ciclo de decisao corrente -- usado pelo limitador de taxa de
   // guinada de bt::NavigateAction.
   virtual double getFrameDt() const = 0;
};

} // namespace bt
} // namespace xC_130
} // namespace models
} // namespace mixr
