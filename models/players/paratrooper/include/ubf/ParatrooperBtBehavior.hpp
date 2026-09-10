#pragma once

#include "mixr/base/ubf/AbstractBehavior.hpp"

#include "behaviortree_cpp_v3/bt_factory.h"

#include "bt/DecisionContext.hpp"
#include "domain/ParachuteFsm.hpp"

#include <string>

namespace mixr {
namespace base { class Distance; class Number; class String; }

namespace models {
namespace xparatrooper {

//------------------------------------------------------------------------------
// Class: ParatrooperBtBehavior
//
// Description: A DECISAO do UBF -- carrega a arvore de comportamento deste
//              modelo, ADIANTA a FSM de estagio (domain::next()) uma vez por
//              ciclo, tica a arvore e devolve uma acao rotulada com o
//              estagio vencedor.
//
// Factory name: ParatrooperBtBehavior
//
// Slots:
//    treeFile       <String>   ! caminho do .xml da arvore
//    deployAgl      <Distance> ! abre o paraquedas nesta AGL, ou abaixo (default: 300 m)
//    groundAgl      <Distance> ! considera pousado nesta AGL, ou abaixo (default: 2 m)
//    requireTerrain <Number>   ! so avanca a FSM com terreno valido (default: true/1)
//    vote           <Number>   ! (herdado de AbstractBehavior)
//
// O AVANCO da FSM mora AQUI, uma vez por chamada de genAction(), NUNCA num
// no da arvore -- ver bt/DecisionContext.hpp para o porque.
//
// 'requireTerrain': Player::reset() zera 'tElev'/'tElevValid', e a primeira
// atualizacao de terreno so acontece no primeiro updateData() de FUNDO
// (10 Hz) -- ANTES disso, getAltitudeAglM() devolve a altitude MSL crua (uma
// SUPERESTIMATIVA da AGL real). Com o gate ligado (default), o primeiro
// ciclo de decisao antes do terreno validar NAO avanca o estagio (fica onde
// estava -- FREEFALL no primeiro frame), evitando uma transicao espuria
// baseada num numero que ainda nao e' AGL de verdade. Desligar (0/false) e'
// para bancada/teste sem WorldModel/terreno nenhum (ver tests/native/).
//
// 'stage_' e' ESTADO PROPRIO deste comportamento, persistido entre chamadas
// -- cada entidade do cenario tem sua PROPRIA instancia de
// ( ParatrooperBtBehavior ... ), entao este campo nunca e' compartilhado
// entre paraquedistas (achado confirmado com o escalonamento por altitude da
// poc de demonstracao -- ver src/poc/paratrooper-drop).
//------------------------------------------------------------------------------
class ParatrooperBtBehavior final : public base::ubf::AbstractBehavior,
                                    public bt::DecisionContext
{
   DECLARE_SUBCLASS(ParatrooperBtBehavior, base::ubf::AbstractBehavior)

public:
   ParatrooperBtBehavior();

   base::ubf::AbstractAction* genAction(const base::ubf::AbstractState* const state,
                                        const double dt) override;

   void reset() override;

   // --- bt::DecisionContext ---------------------------------------------
   domain::Stage stage() const override      { return stage_; }
   bt::JumpDecision& decision() override     { return decision_; }

private:
   void buildTree();

   bool setSlotTreeFile(const base::String* const);
   bool setSlotDeployAgl(const base::Distance* const);
   bool setSlotGroundAgl(const base::Distance* const);
   bool setSlotRequireTerrain(const base::Number* const);

   domain::Stage stage_{domain::Stage::FREEFALL};
   domain::JumpProfile profile_{};
   bool requireTerrain_{true};
   bt::JumpDecision decision_{};

   std::string treeFile_;
   BT::BehaviorTreeFactory btFactory_{};
   BT::Tree tree_{};
   bool treeBuilt_{false};
   bool treeValid_{false};
};

} // namespace xparatrooper
} // namespace models
} // namespace mixr
