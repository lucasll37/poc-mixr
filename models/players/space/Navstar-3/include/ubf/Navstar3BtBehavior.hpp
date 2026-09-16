#pragma once

#include "mixr/base/ubf/AbstractBehavior.hpp"

#include "behaviortree_cpp_v3/bt_factory.h"

#include "bt/DecisionContext.hpp"
#include "domain/CircularOrbit.hpp"
#include "domain/EclipseGeometry.hpp"

#include <string>

namespace mixr {
namespace base { class Angle; class Distance; class String; }

namespace models {
namespace xNavstar_3 {

//------------------------------------------------------------------------------
// Class: Navstar3BtBehavior
//
// Description: A DECISAO do UBF -- propaga a orbita (domain::CircularOrbit),
//              calcula sol/sombra (domain::EclipseGeometry), tica a arvore de
//              comportamento uma vez por ciclo e devolve uma
//              ubf::Navstar3Action carregando a posicao/velocidade ECEF
//              calculadas e o rotulo que a arvore decidiu.
//
// Factory name: Navstar3BtBehavior
//
// Slots:
//    treeFile          <String>  ! caminho do .xml da arvore
//    altitude          <Distance>! altitude acima do raio equatorial (default: 20.180.000 m -- MEO tipico de GPS)
//    inclination       <Angle>   ! inclinacao da orbita (default: 55 deg)
//    raan              <Angle>   ! ascensao reta do nodo ascendente em t=0 (default: 0 deg)
//    argLat0           <Angle>   ! argumento de latitude (fase orbital) em t=0 (default: 0 deg)
//    sunRightAscension <Angle>   ! direcao do sol (fixa, inercial) -- ascensao reta (default: 0 deg)
//    sunDeclination    <Angle>   ! direcao do sol (fixa, inercial) -- declinacao (default: 0 deg)
//    vote              <Number>  ! (herdado de AbstractBehavior)
//
// SIMPLIFICACAO DELIBERADA: sunRightAscension/sunDeclination sao uma direcao
// FIXA -- este modelo nao deriva a posicao do sol a partir de data/hora (o
// MIXR nao tem efemeride solar nenhuma). Sem variacao sazonal: quem quiser
// ver a orbita cruzar de SUNLIT para ECLIPSE escolhe uma direcao de sol que
// produza a geometria desejada para a orbita configurada. Ver
// docs/ARCHITECTURE.md.
//
// 'simTimeS_' e' TEMPO SIMULADO acumulado por 'dt' a cada chamada de
// genAction() -- NUNCA relogio de parede. E' o que mantem a orbita
// reproduzivel entre 1/2/4 threads de tempo critico (mesma sequencia de dt
// em '-deterministic', qualquer numero de threads).
//------------------------------------------------------------------------------
class Navstar3BtBehavior final : public base::ubf::AbstractBehavior,
                                 public bt::DecisionContext
{
   DECLARE_SUBCLASS(Navstar3BtBehavior, base::ubf::AbstractBehavior)

public:
   Navstar3BtBehavior();

   base::ubf::AbstractAction* genAction(const base::ubf::AbstractState* const state,
                                        const double dt) override;

   void reset() override;

   // --- bt::DecisionContext ---------------------------------------------
   domain::SunState sunState() const override        { return sunState_; }
   bt::Navstar3Decision& decision() override         { return decision_; }

private:
   void buildTree();

   bool setSlotTreeFile(const base::String* const);
   bool setSlotAltitude(const base::Distance* const);
   bool setSlotInclination(const base::Angle* const);
   bool setSlotRaan(const base::Angle* const);
   bool setSlotArgLat0(const base::Angle* const);
   bool setSlotSunRightAscension(const base::Angle* const);
   bool setSlotSunDeclination(const base::Angle* const);

   domain::OrbitElements orbit_{};
   double sunRaDeg_{0.0};
   double sunDecDeg_{0.0};

   double simTimeS_{0.0};
   domain::SunState sunState_{domain::SunState::SUNLIT};
   bt::Navstar3Decision decision_{};

   // resultado do ULTIMO ciclo, o que ubf::Navstar3Action recebe e aplica ao
   // Player. 'havePreviousEcef_' distingue "primeiro ciclo, sem velocidade
   // calculavel ainda" de "velocidade zero de verdade".
   double ecefXM_{}, ecefYM_{}, ecefZM_{};
   double ecefVelXMps_{}, ecefVelYMps_{}, ecefVelZMps_{};
   double prevEcefXM_{}, prevEcefYM_{}, prevEcefZM_{};
   bool havePreviousEcef_{false};

   std::string treeFile_;
   BT::BehaviorTreeFactory btFactory_{};
   BT::Tree tree_{};
   bool treeBuilt_{false};
   bool treeValid_{false};
};

} // namespace xNavstar_3
} // namespace models
} // namespace mixr
