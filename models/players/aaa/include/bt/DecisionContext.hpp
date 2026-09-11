#pragma once

#include "bt/NodeContext.hpp"
#include "domain/DomePolicy.hpp"

#include <string>

namespace mixr {
namespace models {
namespace xaaa {
namespace bt {

//------------------------------------------------------------------------------
// DecisionContext -- o que um no da arvore precisa da percepcao/decisao do
// comportamento que o hospeda, e NADA ALEM DISSO.
//
// POR QUE UMA INTERFACE, e nao um ponteiro para ubf::AaaBehavior direto: os
// nos incluiriam "ubf/AaaBehavior.hpp", e com ele viria o MIXR inteiro
// (AbstractBehavior -> Component -> Object). Com a interface, src/bt/**.cpp
// compila contra BehaviorTree.CPP + domain/ apenas, e tests/tree/ carrega a
// arvore de PRODUCAO contra um contexto falso, sem simulacao nenhuma. Mesmo
// desenho de bt_nodes::DecisionContext em models/players/A-4 e do template
// que originou este scaffold.
//------------------------------------------------------------------------------
class DecisionContext
{
public:
   virtual ~DecisionContext() = default;

   // percepcao do frame (o que ubf::AaaState mediu, via o radar de
   // aquisicao proprio da antiaerea + o cabide de municao)
   virtual bool hasTarget() const = 0;
   virtual const std::string& targetName() const = 0;
   virtual double targetRangeM() const = 0;
   virtual bool weaponReady() const = 0;
   virtual const domain::Dome& dome() const = 0;

   // o que a arvore preenche neste tick
   virtual AaaDecision& decision() = 0;
};

} // namespace bt
} // namespace xaaa
} // namespace models
} // namespace mixr
