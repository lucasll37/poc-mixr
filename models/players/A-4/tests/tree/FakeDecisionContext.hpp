#pragma once

// O contexto que os nos da arvore enxergam, montado a mao.
//
// E a peca que o refactor de bt/DecisionContext.hpp destravou: antes os nos
// so existiam pendurados num mixr::models::xnative::BtBehavior, e exercitar a arvore
// exigia subir uma Station inteira. Aqui os campos sao publicos e o teste
// escreve neles direto -- a arvore carregada e a DE PRODUCAO.
//
// Note que threat NAO e um dublê: e a domain::ThreatPolicy de verdade,
// alimentada por alimentarPolitica(), que reproduz exatamente o que
// BtBehavior::feedThreatPolicy() faz antes de cada tick. Testar a arvore com
// uma politica falsa esconderia justamente a interacao que interessa.

#include "bt/DecisionContext.hpp"

namespace testing_support {

class FakeDecisionContext : public bt_nodes::DecisionContext
{
public:
   domain::WorldView snap{};
   bt_nodes::FlightDecision dec{};
   domain::PatrolPlan patrol{};
   domain::RtbPlan rtb{};
   domain::AerobaticPlan aerobatic{};
   domain::ThreatPolicy threat{};

   double frameDt{0.02};
   double fuelReserve{0.35};
   double supportSpeedKts{180.0};
   // Mesmo default de ubf::BtTuning::terrainClearanceM -- ver o comentario
   // de clampAltitudeToTerrain() abaixo.
   double terrainClearanceM{500.0};

   const domain::WorldView& snapshot() const override        { return snap; }
   bt_nodes::FlightDecision& decision() override             { return dec; }
   domain::PatrolPlan& patrolPlan() override                 { return patrol; }
   domain::RtbPlan& rtbPlan() override                       { return rtb; }
   domain::AerobaticPlan& aerobaticPlan() override           { return aerobatic; }
   const domain::ThreatPolicy& threatPolicy() const override { return threat; }
   double getFrameDt() const override                        { return frameDt; }
   double getFuelReserve() const override                    { return fuelReserve; }
   double getSupportSpeedKts() const override                { return supportSpeedKts; }

   // Copia fiel de BtBehavior::clampAltitudeToTerrain() (ver o comentario
   // dela e o de bt/DecisionContext.hpp) -- MESMO piso absoluto de
   // domain::ThreatPolicy.cpp/BtBehavior.cpp (200.0), duplicado pelo MESMO
   // motivo: domain::clampToTerrain() recebe o piso por parametro, de
   // proposito, entao cada dono continua com o proprio numero.
   double clampAltitudeToTerrain(const double altitudeM) const override
   {
      domain::GroundReference ground;
      ground.valid = snap.terrainValid;
      ground.elevationM = snap.terrainElevM;
      return domain::clampToTerrain(altitudeM, ground, terrainClearanceM, 200.0);
   }

   // Copia fiel de BtBehavior::feedThreatPolicy(): Snapshot -> domain.
   void alimentarPolitica(const double dt)
   {
      domain::ThreatContact contact;
      contact.rangeM = snap.contactRangeM;
      contact.relBearingDeg = snap.contactRelBearingDeg;
      contact.deltaAltM = snap.contactDeltaAltM;

      domain::GroundReference ground;
      ground.valid = snap.terrainValid;
      ground.elevationM = snap.terrainElevM;

      threat.update(dt, snap.hasContact, contact, snap.headingDeg, snap.altitudeM, ground);
   }

   // Mesma configuracao que o cenario da falcon1 aplica pelos slots do EDL.
   void configurarComoNoCenario()
   {
      patrol.configure(90.0, 60.0, 90.0, 1750.0, 160.0);
      rtb.configure(0.0, 0.0, 2.0 * 1852.0, 2050.0, 170.0);

      domain::EvasionLimits lim;
      lim.breakTurnDeg = 110.0;
      lim.climbM = 700.0;
      lim.dashSpeedKts = 185.0;
      lim.holdSeconds = 30.0;
      lim.terrainClearanceM = 800.0;
      threat.setLimits(lim);
      threat.reset();

      // Mesmo terrainClearance do bloco acima -- BtTuning::terrainClearanceM
      // e domain::EvasionLimits::terrainClearanceM sao o MESMO slot do EDL
      // (BtBehavior::configurePlans() alimenta os dois com tune.terrainClearanceM).
      terrainClearanceM = lim.terrainClearanceM;
   }
};

} // namespace testing_support
