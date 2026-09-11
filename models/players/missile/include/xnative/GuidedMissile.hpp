#pragma once

#include "domain/Guidance.hpp"

#include "mixr/models/player/weapon/Missile.hpp"

namespace mixr {
namespace models {
namespace xmissile {

//------------------------------------------------------------------------------
// GuidedMissile -- subclasse CINEMATICA de mixr::models::Missile: guiagem
// por navegacao proporcional de verdade (domain::proportionalNavigation),
// sem dynamicsModel nenhum.
//
// weaponGuidance()/weaponDynamics() (os dois protected virtuals que Missile
// ja declara para isto) so' sao chamados por AbstractWeapon::dynamics()
// quando getDynamicsModel() == nullptr -- confirmado lendo
// contexts/src/mixr/src/models/player/weapon/AbstractWeapon.cpp:289 do
// fork vendorizado, nao supondo. E exatamente esse o caminho que este
// missil usa: nenhum JSBSimModel, nenhuma aeronave nova pra vendorizar.
// Ver docs/ARCHITECTURE.md para o porque desta escolha em vez do padrao
// historico (JSBSimModel + dynamics() proprio) que este modelo tinha antes
// de ser removido do repositorio.
//
// Factory name: GuidedMissile
//------------------------------------------------------------------------------
class GuidedMissile : public Missile
{
   DECLARE_SUBCLASS(GuidedMissile, Missile)

public:
   GuidedMissile();

   const char* getDescription() const override; // "Guided"
   const char* getNickname() const override;    // "GuidedMissile"

   void updateTC(const double dt = 0.0) override;

protected:
   void weaponGuidance(const double dt) override;
   void weaponDynamics(const double dt) override;

private:
   // Comando guardado por weaponGuidance(), consumido por weaponDynamics()
   // no MESMO frame -- AbstractWeapon::dynamics() chama os dois em
   // sequencia, sempre juntos, nunca um sem o outro (ver o comentario da
   // classe acima).
   double cmdHeadingRad_{};
   double cmdPitchRad_{};
   double cmdSpeedMps_{};

   domain::FuzeState fuzeState_{};

   // Nada nativo tira um Missile de DETONATED para DELETE_REQUEST --
   // confirmado lendo AbstractWeapon::updateTOF() e Missile::weaponGuidance()
   // do fork vendorizado: os dois so' chamam setMode(DETONATED), nunca
   // DELETE_REQUEST. Sem isto o missil detonado fica pra sempre na lista de
   // players/no Tacview. Acumulado so' na fase 3 (dt*4.0), o MESMO gating
   // de fase que AbstractWeapon::updateTC() ja usa para o TOF.
   double detonatedLingerSec_{};
   static constexpr double kLingerSec{2.0};
};

} // namespace xmissile
} // namespace models
} // namespace mixr
