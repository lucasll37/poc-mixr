#pragma once

// O contexto que os nos da arvore enxergam, montado a mao. Permite carregar
// a arvore DE PRODUCAO (configs/c130_nav_tree.xml) e exercitar
// bt::NavigateAction sem Station/player/simulacao nenhuma.

#include "bt/DecisionContext.hpp"

namespace mixr {
namespace models {
namespace xC_130 {
namespace testing_support {

class FakeDecisionContext : public bt::DecisionContext
{
public:
   domain::WorldView snap{};
   bt::FlightDecision dec{};
   double frameDt{0.02};

   const domain::WorldView& snapshot() const override   { return snap; }
   bt::FlightDecision& decision() override                { return dec; }
   double getFrameDt() const override                     { return frameDt; }
};

} // namespace testing_support
} // namespace xC_130
} // namespace models
} // namespace mixr
