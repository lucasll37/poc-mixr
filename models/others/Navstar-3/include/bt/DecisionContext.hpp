#pragma once

#include "bt/NodeContext.hpp"
#include "domain/EclipseGeometry.hpp"

namespace mixr {
namespace models {
namespace xNavstar_3 {
namespace bt {

//------------------------------------------------------------------------------
// DecisionContext -- o que um no da arvore precisa do comportamento que o
// hospeda, e NADA ALEM DISSO.
//
// POR QUE UMA INTERFACE, e nao um ponteiro para ubf::Navstar3BtBehavior
// direto: os nos incluiriam "ubf/Navstar3BtBehavior.hpp", e com ele viria o
// MIXR inteiro. Com a interface, src/bt/**.cpp compila contra
// BehaviorTree.CPP + domain/ apenas, e tests/tree/ carrega a arvore de
// PRODUCAO contra um contexto falso, sem simulacao nenhuma.
//
// Mantida no MINIMO que este modelo precisa (2 metodos) -- mesmo espirito
// da versao de 3 metodos do C-130 (que ja reduziu a versao de 9 getters do
// A-4 por nao ter logica de combate): sem combustivel, sem alerta, sem
// contato -- so o estado sol/sombra ja computado neste ciclo, e o lugar
// para a arvore escrever a decisao.
//------------------------------------------------------------------------------
class DecisionContext
{
public:
   virtual ~DecisionContext() = default;

   // o estado sol/sombra JA CALCULADO neste ciclo (ver
   // ubf::Navstar3BtBehavior::genAction(), que chama domain::sunState()
   // antes de tickar a arvore) -- os nos so LEEM, nunca recalculam.
   virtual domain::SunState sunState() const = 0;

   // o que a arvore preenche neste tick
   virtual Navstar3Decision& decision() = 0;
};

} // namespace bt
} // namespace xNavstar_3
} // namespace models
} // namespace mixr
