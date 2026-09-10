#pragma once

#include <string>

namespace mixr {
namespace models {
namespace xNavstar_3 {
namespace bt {

//------------------------------------------------------------------------------
// Navstar3Decision -- o que a arvore PRODUZ num tick.
//
// Os nos nao tocam em nenhum objeto MIXR: eles so preenchem esta estrutura.
// Quem a transforma em atuacao e ubf::Navstar3Action.
//------------------------------------------------------------------------------
struct Navstar3Decision
{
   bool taken{};
   std::string label{"SUNLIT"};

   void reset() { *this = Navstar3Decision{}; }

   void take(const std::string& text)
   {
      taken = true;
      label = text;
   }
};

// Dependencia fixa dos nos: o comportamento que os hospeda. Entra pelo
// CONSTRUTOR do no, via factory.registerBuilder<T>(ID, builder) -- convencao
// do BehaviorTree.CPP v3 para argumentos extras.
//
// O ponteiro e para a INTERFACE (bt/DecisionContext.hpp), nao para a classe
// concreta: e o que mantem os nos compilaveis sem o MIXR.
class DecisionContext;

struct NodeContext
{
   DecisionContext* behavior{};
};

} // namespace bt
} // namespace xNavstar_3
} // namespace models
} // namespace mixr
