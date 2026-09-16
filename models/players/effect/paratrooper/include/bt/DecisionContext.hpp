#pragma once

#include "bt/NodeContext.hpp"
#include "domain/ParachuteFsm.hpp"

namespace mixr {
namespace models {
namespace xparatrooper {
namespace bt {

//------------------------------------------------------------------------------
// DecisionContext -- o que um no da arvore precisa do comportamento que o
// hospeda, e NADA ALEM DISSO.
//
// So DUAS virtuais puras, deliberadamente menos que o template (4): o AVANCO
// da FSM (domain::next()) NAO acontece aqui dentro -- acontece uma vez so,
// em ubf::ParatrooperBtBehavior::genAction(), ANTES de tickar a arvore. Com
// ate DUAS condicoes avaliadas no mesmo tick (o pior caso, FREEFALL, tenta
// "esta LANDED?" e depois "esta CANOPY?"), deixar um NO avancar o estagio (o
// que o ExampleThresholdCondition do template faz, com seguranca, so porque
// a arvore dele nunca avalia mais de UMA condicao por tick) duplicaria o
// avanco. Os nos daqui, portanto, so LEEM 'stage()' -- nunca mutam.
//
// POR QUE UMA INTERFACE, e nao um ponteiro para ubf::ParatrooperBtBehavior
// direto: os nos incluiriam "ubf/ParatrooperBtBehavior.hpp", e com ele viria
// o MIXR inteiro. Com a interface, src/bt/**.cpp compila contra
// BehaviorTree.CPP + domain/ apenas, e tests/tree/ carrega a arvore de
// PRODUCAO contra um contexto falso, sem simulacao nenhuma.
//------------------------------------------------------------------------------
class DecisionContext
{
public:
   virtual ~DecisionContext() = default;

   // o estagio JA AVANCADO neste ciclo (ver o comentario acima) -- leitura
   // pura, nunca escrita por um no.
   virtual domain::Stage stage() const = 0;

   // o que a arvore preenche neste tick
   virtual JumpDecision& decision() = 0;
};

} // namespace bt
} // namespace xparatrooper
} // namespace models
} // namespace mixr
