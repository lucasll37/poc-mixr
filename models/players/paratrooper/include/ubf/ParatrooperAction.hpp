#pragma once

#include "mixr/base/ubf/AbstractAction.hpp"

#include <string>

namespace mixr {
namespace base { class Component; }

namespace models {
namespace xparatrooper {

//------------------------------------------------------------------------------
// Class: ParatrooperAction
//
// Description: A ATUACAO do UBF -- traduz o rotulo decidido pela arvore
//              (FREEFALL/CANOPY/LANDED) num comando de verdade sobre o
//              proprio Paratrooper (setJumpStage()), e escreve as duas
//              obrigacoes do xboard.
//
// Factory name: ParatrooperAction
//
// Slots: (nenhum -- o rotulo chega pronto do comportamento que a criou)
//
// O 'dynamic_cast<Paratrooper*>' dentro de execute() e' seguro porque as
// duas classes moram na MESMA .so -- nao ha risco de RTTI cruzando
// fronteira de plugin aqui (diferente de, por exemplo, um dynamic_cast
// contra uma classe de OUTRO modelo).
//
// O que execute() NAO PODE deixar de fazer, em QUALQUER modelo, e escrever
// no xboard -- e' a UNICA obrigacao que falha em SILENCIO: sem ela, o host
// sobe, o cenario parseia, tudo passa, e a tela de status/o dump
// '-deterministic' mostram 'bt=--'/'dec=0' para sempre, sem erro nenhum.
// Por isso as duas chamadas rodam mesmo que o dynamic_cast para
// xnative::Paratrooper falhe (um EDL malconfigurado fica VISIVEL no
// dashboard, em vez de simplesmente nao comandar nada em silencio).
//------------------------------------------------------------------------------
class ParatrooperAction final : public base::ubf::AbstractAction
{
   DECLARE_SUBCLASS(ParatrooperAction, base::ubf::AbstractAction)

public:
   ParatrooperAction();
   explicit ParatrooperAction(const std::string& label);

   bool execute(base::Component* actor) override;

private:
   std::string label{"FREEFALL"};
};

} // namespace xparatrooper
} // namespace models
} // namespace mixr
