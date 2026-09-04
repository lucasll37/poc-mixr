#pragma once

#include "mixr/models/player/weapon/Missile.hpp"
#include "events/payloads/EID_ALERT/TacticalAlert.hpp"

namespace mixr {
namespace xmissile {

//------------------------------------------------------------------------------
// Class: GuidedMissile
//
// Description: O "modelo novo" desta demo -- um mixr::models::Missile de
// verdade, guiado por lei propria (domain::Guidance, perseguicao pura) sobre
// um mixr::models::JSBSimModel anexado como dynamicsModel: fisica 6-DOF de
// verdade, nao o modelo cinematico simplificado que Missile:: ja traz.
//
// Factory name: GuidedMissile
// Slots: (nenhum -- usa os que AbstractWeapon/Missile/Player ja tem:
//         maxTOF, lethalRange, maxBurstRng, id, side, type, signature,
//         dataLogTime, components: { dynamicsModel: ( JSBSimModel ... ) })
//
// POR QUE dynamics() E NAO weaponGuidance()/weaponDynamics(): confirmado lendo
// AbstractWeapon::dynamics() (contexts/src/mixr/src/models/player/weapon/
// AbstractWeapon.cpp) -- esses dois hooks so sao chamados quando
// getDynamicsModel() == nullptr (e o modelo cinematico embutido de Missile::
// que os usa). Com um JSBSimModel anexado eles NUNCA rodam; a integracao vai
// por Player::dynamics() -> DynamicsModel::updateModel(), chamada de dentro
// de AbstractWeapon::dynamics() -> BaseClass::dynamics(dt). Por isso a
// guiagem entra ANTES desse BaseClass::dynamics(dt): calcula o comando e
// escreve no FCS do JSBSim (setControlStickRollInput/PitchInput), so entao
// deixa o framework integrar o frame com esse comando ja aplicado.
//
// DETONACAO -> DESTRUICAO: collisionNotification()/crashNotification() (de
// AbstractWeapon) ja levam o mode a DETONATED -- mas DETONATED sozinho nao
// tira o player da lista da simulacao (so AbstractWeapon::reset() faz essa
// transicao para DELETE_REQUEST, e so num reset de cenario inteiro). Aqui, um
// pequeno timer de permanencia (kLingerSec) faz esse passo que falta, depois
// de detonar.
//
// TRATAMENTO DE EVENTO CUSTOMIZADO (events::EID_ALERT) -- prova de que um
// evento definido uma vez em events/ e tratado por um handler escrito
// DEPOIS, num plugin sem nenhuma relacao de compilacao com quem emite
// (xnative::AlertDatalink, em models/flight). Ver events/README.md. Efeito
// deliberadamente trivial (so log via shared/xlog) -- o ponto e a fiacao do
// evento, nao dar ao missil uma tatica nova.
//
// ARMADILHA CONFIRMADA RODANDO: esse timer NAO pode morar em dynamics().
// Player::updateTC() so chama dynamics() quando mode == ACTIVE || PRE_RELEASE
// (mesmo gate documentado no CLAUDE.md) -- uma vez DETONATED, dynamics()
// simplesmente PARA de ser chamado, e um timer ali nunca avanca (medido:
// o player ficava "detonated" para sempre no dump, sem nunca virar
// DELETE_REQUEST). AbstractWeapon::updateTC() e diferente: ele chama
// BaseClass::updateTC(dt) (que tem o mesmo gate, mas so afeta o QUE RODA
// DENTRO dele) e DEPOIS roda a propria logica de fase (transicao
// PRE_RELEASE->ACTIVE, TOF) SEM checar o mode do missil -- e por isso o
// framework consegue transicionar um weapon de PRE_RELEASE para ACTIVE em
// primeiro lugar. O timer de destruicao entra no mesmo lugar, pelo mesmo
// motivo: updateTC() continua sendo chamado mesmo com o player DETONATED.
//------------------------------------------------------------------------------
class GuidedMissile : public models::Missile
{
   DECLARE_SUBCLASS(GuidedMissile, models::Missile)

public:
   GuidedMissile();

   const char* getDescription() const override;
   const char* getNickname() const override;
   int getCategory() const override;

   bool collisionNotification(models::Player* const p) override;
   bool crashNotification() override;

   void updateTC(const double dt = 0.0) override;

   bool event(const int event, base::Object* const obj = nullptr) override;

protected:
   void dynamics(const double dt) override;

private:
   void guide(const double dt);

   bool onAlertEvent(events::TacticalAlert* const);

   static constexpr double kLingerSec{2.0};

   // Limite de taxa de variacao do comando (unidades normalizadas por
   // segundo) -- ver o "porque" no cabecalho: sem isto, o comando P puro de
   // domain::pursuit() satura em +-1 de um frame para o outro, e a inercia
   // reduzida do aim1.xml responde rapido demais para um passo de 0.02s --
   // medido divergindo (>100 graus de banco em 2 frames, velocidade > Mach 4
   // em 0.2s). O amortecimento certo seria taxa de rolagem/arfagem
   // realimentada; isto e o minimo que evita a divergencia numerica.
   static constexpr double kMaxSlewPerSec{4.0};

   double detonatedTof_{-1.0};   // segundos desde a detonacao; -1 = ainda voando
   double lastRollNorm_{};
   double lastPitchNorm_{};
};

} // namespace xmissile
} // namespace mixr
