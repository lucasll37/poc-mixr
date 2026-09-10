#pragma once

#include "mixr/base/ubf/AbstractAction.hpp"

#include <string>

namespace mixr {
namespace base { class Component; }

namespace models {
namespace xtemplate {

//------------------------------------------------------------------------------
// Class: ExampleAction
//
// Description: A ATUACAO do UBF -- o que de fato acontece quando o
//              comportamento vencedor e executado.
//
// Factory name: ExampleAction
//
// Slots: (nenhum -- o rotulo chega pronto do comportamento que a criou, ver
//        ubf::ExampleBehavior::genAction())
//
// Esta classe NAO comanda nenhum subsistema do player -- de proposito, para
// o exemplo compilar e rodar contra QUALQUER tipo de Player, nao so
// aeronaves. Um modelo real substituiria o corpo de execute() por comandos
// de verdade (ex.: models::Autopilot::setCommandedHeadingD(), como
// models/players/A-4/src/ubf/FlightAction.cpp ou
// ../mirror.cpp (o mirror de contrato deste mesmo diretorio) fazem -- leia
// os dois quando for a hora de sair do "engajado/nao engajado" para algo
// que pilota de verdade).
//
// O que execute() NAO PODE deixar de fazer, em QUALQUER modelo, e escrever
// no xboard -- ver o comentario dentro do .cpp e
// models/template/docs/CONTRATO.md secao 3. E a UNICA obrigacao de
// um modelo que falha em SILENCIO: sem ela, o host sobe, o cenario parseia,
// tudo passa, e a tela de status/o dump '-deterministic' mostram
// 'bt=--'/'dec=0' para sempre, sem erro nenhum em lugar nenhum.
//------------------------------------------------------------------------------
class ExampleAction final : public base::ubf::AbstractAction
{
   DECLARE_SUBCLASS(ExampleAction, base::ubf::AbstractAction)

public:
   ExampleAction();
   explicit ExampleAction(const std::string& label);

   bool execute(base::Component* actor) override;

private:
   std::string label{"IDLE"};
};

} // namespace xtemplate
} // namespace models
} // namespace mixr
