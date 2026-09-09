#pragma once

#include "bt/NodeContext.hpp"
#include "domain/ExampleThreshold.hpp"

namespace mixr {
namespace models {
namespace xtemplate {
namespace bt {

//------------------------------------------------------------------------------
// DecisionContext -- o que um no da arvore precisa do comportamento que o
// hospeda, e NADA ALEM DISSO.
//
// POR QUE UMA INTERFACE, e nao um ponteiro para ubf::ExampleBehavior direto:
// os nos incluiriam "ubf/ExampleBehavior.hpp", e com ele viria o MIXR
// inteiro (AbstractBehavior -> Component -> Object). O efeito pratico seria
// que a arvore -- a peca mais propria do seu modelo -- so poderia ser
// exercitada subindo uma Station. Com a interface, src/bt/**.cpp compila
// contra BehaviorTree.CPP + domain/ apenas, e tests/tree/ carrega a arvore
// de PRODUCAO contra um contexto falso, sem simulacao nenhuma.
//
// E o mesmo desenho de bt_nodes::DecisionContext em models/players/A-4 (nove
// getters la, quatro aqui). Acrescente um metodo por grandeza nova que um no
// seu precise ler -- ExampleBehavior o implementa sem escrever metodo novo,
// porque as assinaturas ja sao as dos membros dele.
//------------------------------------------------------------------------------
class DecisionContext
{
public:
   virtual ~DecisionContext() = default;

   // percepcao do frame (a leitura crua que ubf::ExampleState produziu)
   virtual double value() const = 0;

   // a regra pura, com os limiares que vieram do .edl
   virtual const domain::ExampleThreshold& rule() const = 0;

   // o estado do gatilho, que SOBREVIVE entre ticks -- quem avanca e o no
   // (ver src/bt/nodes/ExampleThresholdCondition.cpp), quem guarda e o
   // comportamento. Sem isso a histerese nao existiria: cada tick
   // reavaliaria so contra onValue.
   virtual bool engaged() const = 0;
   virtual void setEngaged(bool) = 0;

   // o que a arvore preenche neste tick
   virtual ExampleDecision& decision() = 0;
};

} // namespace bt
} // namespace xtemplate
} // namespace models
} // namespace mixr
