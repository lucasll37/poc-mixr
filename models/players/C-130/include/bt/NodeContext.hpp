#pragma once

#include "domain/FlightCommand.hpp"

#include <string>

namespace mixr {
namespace models {
namespace xC_130 {
namespace bt {

//------------------------------------------------------------------------------
// FlightDecision -- o que a arvore PRODUZ num tick.
//
// Os nos nao tocam em nenhum objeto MIXR: eles so preenchem esta estrutura.
// Quem a transforma em atuacao e ubf::FlightAction. Versao ENXUTA do
// bt_nodes::FlightDecision de models/players/A-4 -- sem broadcastAlert nem
// campos de contato: este modelo so navega, nunca alerta ninguem.
//------------------------------------------------------------------------------
struct FlightDecision
{
   bool taken{};
   domain::FlightCommand command{};
   std::string label{"?"};

   void reset() { *this = FlightDecision{}; }

   void take(const domain::FlightCommand& cmd, const std::string& text)
   {
      taken = true;
      command = cmd;
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
} // namespace xC_130
} // namespace models
} // namespace mixr
