#pragma once

#include "domain/FlightCommand.hpp"

#include <string>

namespace mixr { namespace xair { class BtBehavior; } }

namespace bt_nodes {

//------------------------------------------------------------------------------
// FlightDecision -- o que a arvore PRODUZ num tick.
//
// Os nos nao tocam em nenhum objeto MIXR: eles so preenchem esta estrutura.
// Quem transforma isto em atuacao e a xair::FlightAction do UBF. Assim o
// mesmo conjunto de nos serviria a outra aeronave, outro atuador, ou a um
// teste unitario sem simulacao nenhuma.
//------------------------------------------------------------------------------
struct FlightDecision
{
   bool taken{};
   domain::FlightCommand command{};
   std::string label{"?"};

   // pedido de transmissao do alerta tatico para os outros avioes
   bool broadcastAlert{};
   std::string alertContactName;
   double alertNorthM{};
   double alertEastM{};
   double alertAltitudeM{};
   double alertRangeM{};

   void reset() { *this = FlightDecision{}; }

   void take(const domain::FlightCommand& cmd, const std::string& text)
   {
      taken = true;
      command = cmd;
      label = text;
   }
};

// Dependencia fixa dos nos: o comportamento que os hospeda (dono do
// snapshot, dos planos de voo e da decisao). Entra pelo CONSTRUTOR do no,
// via factory.registerBuilder<T>(ID, builder) -- convencao do
// BehaviorTree.CPP v3 para argumentos extras (o blackboard e para dados
// que fluem ENTRE nos, nao para injecao de dependencia).
struct NodeContext
{
   mixr::xair::BtBehavior* behavior{};
};

} // namespace bt_nodes
