#pragma once

#include <string>

#include "domain/ParachuteFsm.hpp"

namespace mixr {
namespace models {
namespace xparatrooper {
namespace bt {

//------------------------------------------------------------------------------
// JumpDecision -- o que a arvore PRODUZ num tick.
//
// Os nos nao tocam em nenhum objeto MIXR: eles so preenchem esta estrutura.
// Quem a transforma em atuacao e ubf::ParatrooperAction.
//------------------------------------------------------------------------------
struct JumpDecision
{
   bool taken{};
   std::string label{domain::labelOf(domain::Stage::FREEFALL)};

   void reset() { *this = JumpDecision{}; }

   void take(const std::string& text)
   {
      taken = true;
      label = text;
   }
};

// Dependencia fixa dos nos: o comportamento que os hospeda (dono da
// percepcao, do estagio que sobrevive entre ticks e da decisao). Entra pelo
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
} // namespace xparatrooper
} // namespace models
} // namespace mixr
