#pragma once

#include "mixr/base/ubf/AbstractBehavior.hpp"

#include "bt/DecisionContext.hpp"
#include "ubf/FlightState.hpp"

#include "behaviortree_cpp_v3/bt_factory.h"

#include <string>

namespace mixr {
namespace base { class String; }

namespace models {
namespace xC_130 {

//------------------------------------------------------------------------------
// Class: BtBehavior
//
// Description: A DECISAO -- um comportamento do UBF cuja politica interna e
//              uma arvore do BehaviorTree.CPP de UM NO SO (Navigate, sem
//              Fallback, sem combate/evasao -- este modelo so navega).
//
// Factory name: C130BtBehavior
//
// Slots:
//    treeFile <String> ! caminho do .xml da arvore (obrigatorio na pratica --
//                        sem ele o modelo nao decide nada, ver genAction())
//    vote     <Number> ! herdado de AbstractBehavior
//
// COMO UBF E BehaviorTree.CPP SE ENCAIXAM: o UBF define TRES papeis --
// percepcao (AbstractState), decisao (AbstractBehavior) e atuacao
// (AbstractAction) -- mas nao diz COMO decidir. Esta classe preenche o
// papel de decisao delegando a politica a uma arvore: genAction() tica a
// arvore, a arvore preenche um bt::FlightDecision, e o resultado vira uma
// FlightAction ref-contada com o VOTO deste comportamento. genAction() pode
// devolver nullptr (a arvore nao decidiu nada) -- o UBF trata isso como
// "sem recomendacao", nao como erro.
//
// Bem mais simples que o BtBehavior de models/players/A-4: sem PatrolPlan/
// RtbPlan/ThreatPolicy, sem clampAltitudeToTerrain, sem monitor do Groot --
// nao ha o que orquestrar alem de construir e ticar a arvore.
//------------------------------------------------------------------------------
class BtBehavior final : public base::ubf::AbstractBehavior, public bt::DecisionContext
{
   DECLARE_SUBCLASS(BtBehavior, base::ubf::AbstractBehavior)

public:
   BtBehavior();

   base::ubf::AbstractAction* genAction(const base::ubf::AbstractState* const state,
                                        const double dt) override;

   void reset() override;

   // --- bt::DecisionContext -----------------------------------------------
   const domain::WorldView& snapshot() const override   { return snap; }
   bt::FlightDecision& decision() override               { return currentDecision; }
   double getFrameDt() const override                    { return frameDt; }

protected:
   bool shutdownNotification() override;

private:
   void buildTree();

   FlightState::Snapshot snap;
   bt::FlightDecision currentDecision;

   BT::BehaviorTreeFactory btFactory;
   BT::Tree tree;
   bool treeBuilt{};
   bool treeValid{};
   double frameDt{};

   std::string treeFile_;

   bool setSlotTreeFile(const base::String* const);
};

} // namespace xC_130
} // namespace models
} // namespace mixr
