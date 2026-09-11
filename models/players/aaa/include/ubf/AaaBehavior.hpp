#pragma once

#include "mixr/base/ubf/AbstractBehavior.hpp"

#include "behaviortree_cpp_v3/bt_factory.h"

#include "bt/DecisionContext.hpp"
#include "domain/DomePolicy.hpp"

#include <string>

namespace mixr {
namespace base { class String; }

namespace models {
namespace xaaa {

//------------------------------------------------------------------------------
// Class: AaaBehavior
//
// Description: A DECISAO do UBF -- carrega a arvore de comportamento
//              (BehaviorTree.CPP v3), tica uma vez por ciclo e devolve uma
//              AaaAction rotulada com o que a arvore decidiu.
//
// Factory name: AaaBehavior
//
// Slots:
//    treeFile <String>   ! caminho do .xml da arvore (obrigatorio na pratica --
//                          sem ele a antiaerea nunca dispara, ver o .cpp)
//    vote     <Number>   ! (herdado de AbstractBehavior)
//
// PADRAO: percepcao (AaaState) entra, a arvore decide chamando a regra pura
// de domain/DomePolicy.hpp pelos nos de bt/, acao (AaaAction) sai rotulada e
// dispara de verdade. Mesmo desenho em camadas de
// models/players/A-4/include/ubf/BtBehavior.hpp, reduzido ao essencial:
// regra UNICA ("alvo no domo + arma disponivel -> dispara"), sem
// prioridades concorrentes -- por isso a arvore e' um Fallback de DOIS
// ramos (Sequence de disparo, ou observar), nao os quatro ramos
// arbitrados do A-4.
//
// Nao guarda estado de "ja disparei" -- StoresMgr::available() volta a 0
// depois do unico ( GuidedMissile ) do cabide sair, e TargetInDomeCondition
// ja checa isso: o cabide vazio e' o que impede um segundo disparo,
// naturalmente, sem flag nenhuma (mesmo padrao ja usado pelo lancador do
// A-4, ver models/players/A-4/CHANGELOG.md).
//------------------------------------------------------------------------------
class AaaBehavior final : public base::ubf::AbstractBehavior,
                          public bt::DecisionContext
{
   DECLARE_SUBCLASS(AaaBehavior, base::ubf::AbstractBehavior)

public:
   AaaBehavior();

   base::ubf::AbstractAction* genAction(const base::ubf::AbstractState* const state,
                                        const double dt) override;

   void reset() override;

   // --- bt::DecisionContext ---------------------------------------------
   bool hasTarget() const override                   { return hasTarget_; }
   const std::string& targetName() const override    { return targetName_; }
   double targetRangeM() const override               { return targetRangeM_; }
   bool weaponReady() const override                  { return weaponReady_; }
   const domain::Dome& dome() const override           { return dome_; }
   bt::AaaDecision& decision() override                { return decision_; }

private:
   void buildTree();

   bool setSlotTreeFile(const base::String* const);

   bool hasTarget_{};
   std::string targetName_;
   double targetRangeM_{};
   bool weaponReady_{};
   domain::Dome dome_{};
   bt::AaaDecision decision_{};

   std::string treeFile_;
   BT::BehaviorTreeFactory btFactory_{};
   BT::Tree tree_{};
   bool treeBuilt_{false};
   bool treeValid_{false};
};

} // namespace xaaa
} // namespace models
} // namespace mixr
