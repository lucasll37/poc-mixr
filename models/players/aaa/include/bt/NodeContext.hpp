#pragma once

#include <string>

namespace mixr {
namespace models {
namespace xaaa {
namespace bt {

//------------------------------------------------------------------------------
// AaaDecision -- o que a arvore PRODUZ num tick.
//
// Os nos nao tocam em nenhum objeto MIXR: eles so preenchem esta estrutura.
// Quem a transforma em atuacao e' ubf::AaaAction. 'label' existe para que a
// arvore SEMPRE produza uma decisao observavel (bt=WATCHING/FIRE no
// xboard/dump), mesmo nos frames em que nao ha alvo para engajar -- e' o
// mesmo raciocinio do template (ExampleDecision::label), que a esta altura
// e' convencao deste repositorio (ver CONTRATO.md secao 3: nunca deixar
// 'bt=--' por nunca escrever no xboard).
//------------------------------------------------------------------------------
struct AaaDecision
{
   bool fireRequested{};
   std::string targetName;
   std::string label{"WATCHING"};

   void reset() { *this = AaaDecision{}; }

   // Ramo de degradacao/nenhum alvo no domo: continua observando, nunca
   // dispara.
   void watch() { label = "WATCHING"; }

   void fire(const std::string& target)
   {
      fireRequested = true;
      targetName = target;
      label = "FIRE";
   }
};

// Dependencia fixa dos nos: o comportamento que os hospeda. Entra pelo
// CONSTRUTOR do no, via factory.registerBuilder<T>(ID, builder) -- convencao
// do BehaviorTree.CPP v3 para argumentos extras.
//
// O ponteiro e' para a INTERFACE (bt/DecisionContext.hpp), nao para a classe
// concreta: e' o que mantem os nos compilaveis sem o MIXR.
class DecisionContext;

struct NodeContext
{
   DecisionContext* behavior{};
};

} // namespace bt
} // namespace xaaa
} // namespace models
} // namespace mixr
