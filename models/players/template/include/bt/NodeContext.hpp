#pragma once

#include <string>

namespace mixr {
namespace models {
namespace xtemplate {
namespace bt {

//------------------------------------------------------------------------------
// ExampleDecision -- o que a arvore PRODUZ num tick.
//
// Os nos nao tocam em nenhum objeto MIXR: eles so preenchem esta estrutura.
// Quem a transforma em atuacao e ubf::ExampleAction. E o mesmo desenho de
// bt_nodes::FlightDecision em models/players/A-4, reduzido ao minimo -- la a
// struct carrega tambem um domain::FlightCommand (rumo/altitude/velocidade),
// porque aquele modelo pilota de verdade; aqui so o rotulo, porque
// ExampleAction nao comanda subsistema nenhum (ver o cabecalho dela para o
// porque).
//
// Acrescente campos aqui conforme o SEU modelo precisar mandar mais coisa da
// arvore para a atuacao -- e o ponto de extensao natural, e nao exige tocar
// em no nenhum que ja exista.
//------------------------------------------------------------------------------
struct ExampleDecision
{
   bool taken{};
   std::string label{"IDLE"};

   void reset() { *this = ExampleDecision{}; }

   void take(const std::string& text)
   {
      taken = true;
      label = text;
   }
};

// Dependencia fixa dos nos: o comportamento que os hospeda (dono da
// percepcao, do estado que sobrevive entre ticks e da decisao). Entra pelo
// CONSTRUTOR do no, via factory.registerBuilder<T>(ID, builder) -- convencao
// do BehaviorTree.CPP v3 para argumentos extras (o blackboard e para dados
// que fluem ENTRE nos, nao para injecao de dependencia).
//
// O ponteiro e para a INTERFACE (bt/DecisionContext.hpp), nao para a classe
// concreta: e o que mantem os nos compilaveis sem o MIXR -- ver o cabecalho
// daquele arquivo. Aqui basta a declaracao adiantada.
class DecisionContext;

struct NodeContext
{
   DecisionContext* behavior{};
};

} // namespace bt
} // namespace xtemplate
} // namespace models
} // namespace mixr
