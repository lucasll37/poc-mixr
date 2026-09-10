#pragma once

#include "mixr/base/ubf/AbstractBehavior.hpp"

#include "behaviortree_cpp_v3/bt_factory.h"

#include "bt/DecisionContext.hpp"
#include "domain/ExampleThreshold.hpp"

#include <string>

namespace mixr {
namespace base { class Distance; class String; }

namespace models {
namespace xtemplate {

//------------------------------------------------------------------------------
// Class: ExampleBehavior
//
// Description: A DECISAO do UBF -- carrega uma arvore de comportamento
//              (BehaviorTree.CPP v3), tica ela uma vez por ciclo e devolve
//              uma acao rotulada com o que a arvore decidiu.
//
// Factory name: ExampleBehavior
//
// Slots:
//    treeFile <String>   ! caminho do .xml da arvore (obrigatorio na pratica --
//                          sem ele o modelo cai na regra direta, ver o .cpp)
//    onValue  <Distance> ! Engaja quando a leitura sobe ATE ou ALEM disto (default: 1.0 m)
//    offValue <Distance> ! So desengaja quando a leitura cai ABAIXO disto (default: 0.0 m)
//    vote     <Number>   ! (herdado de AbstractBehavior)
//
// PADRAO: percepcao (ExampleState) entra, a arvore decide chamando regras
// puras de domain/ pelos nos de bt/, acao (ExampleAction) sai rotulada e
// escreve no xboard. E' o mesmo desenho de
// models/players/A-4/include/ubf/BtBehavior.hpp, reduzido: dois nos em vez de
// onze, uma arvore de dois ramos em vez de quatro.
//
// POR QUE A ARVORE, e nao um if em C++ (que foi o que este template teve
// ate' ganhar bt/): a forma da decisao passa a ser DADO (configs/*.xml),
// editavel no Groot, versionavel e diffavel, sem recompilar. Enquanto a
// decisao e' uma regra so, o if e' mais simples e honesto -- e por isso a
// regra pura continua existindo, intacta, em domain/ExampleThreshold.hpp. O
// que a arvore acrescenta e' o lugar para a SEGUNDA e a TERCEIRA regra
// entrarem sem virar uma cascata de if aninhado.
//
// 'engaged' e' ESTADO PROPRIO deste comportamento, persistido entre chamadas
// -- e' o que faz o Schmitt trigger funcionar (sem lembrar o estado
// anterior, toda decisao reavaliaria so contra 'onValue', e a histerese nao
// existiria). Quem AVANCA a regra e' o no ( ExampleThreshold ); quem GUARDA
// o valor entre ticks e' esta classe, via a interface bt::DecisionContext.
// Cada entidade do cenario tem sua PROPRIA instancia de
// ( ExampleBehavior ... ), entao este campo nunca e' compartilhado.
//------------------------------------------------------------------------------
class ExampleBehavior final : public base::ubf::AbstractBehavior,
                              public bt::DecisionContext
{
   DECLARE_SUBCLASS(ExampleBehavior, base::ubf::AbstractBehavior)

public:
   ExampleBehavior();

   base::ubf::AbstractAction* genAction(const base::ubf::AbstractState* const state,
                                        const double dt) override;

   void reset() override;

   // --- bt::DecisionContext ---------------------------------------------
   // Nenhum metodo novo: as quatro assinaturas ja eram os membros abaixo.
   double value() const override                          { return value_; }
   const domain::ExampleThreshold& rule() const override  { return rule_; }
   bool engaged() const override                          { return engaged_; }
   void setEngaged(const bool v) override                 { engaged_ = v; }
   bt::ExampleDecision& decision() override               { return decision_; }

private:
   void buildTree();

   bool setSlotOnValue(const base::Distance* const);
   bool setSlotOffValue(const base::Distance* const);
   bool setSlotTreeFile(const base::String* const);

   domain::ExampleThreshold rule_{};
   bool engaged_{false};
   double value_{};
   bt::ExampleDecision decision_{};

   std::string treeFile_;
   BT::BehaviorTreeFactory btFactory_{};
   BT::Tree tree_{};
   bool treeBuilt_{false};
   bool treeValid_{false};
};

} // namespace xtemplate
} // namespace models
} // namespace mixr
