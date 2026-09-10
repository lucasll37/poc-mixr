#pragma once

#include "mixr/models/player/effect/Effect.hpp"

#include "domain/ParachuteFsm.hpp"

namespace mixr {
namespace base { class Distance; class Number; }

namespace models {
namespace xparatrooper {

//------------------------------------------------------------------------------
// Class: Paratrooper
//
// Description: O CORPO fisico deste modelo -- queda livre, paraquedas,
//              pouso. Deriva de mixr::models::Effect (o mesmo idioma nativo
//              de Chaff/Decoy/Flare/o placeholder do C-130) porque a fisica
//              por estagio precisa de um weaponDynamics() proprio, e porque
//              e' o que deixa esta classe um substituto de EDL direto para
//              'C130ParatrooperPlaceholder' (ver models/players/C-130) numa
//              tarefa futura -- ver docs/ARCHITECTURE.md.
//
// Factory name: Paratrooper
//
// Slots:
//    canopyDescentRate  <Number>   ! taxa de descida sob o velame, m/s (default: 5.5)
//    releaseOffsetAft   <Distance> ! quanto ATRAS da aeronave lancadora ele
//                                  ! nasce (default: 15 m)
//    releaseOffsetBelow <Distance> ! quanto ABAIXO da aeronave lancadora ele
//                                  ! nasce (default: 10 m)
//    (mais 'dragIndex' herdado de Effect, e id/side/type/dataLogTime/maxTOF/
//    crashOverride/killOverride herdados de AbstractWeapon/Player)
//
// QUATRO SOBRESCRITAS SOBRE Effect, todas medidas contra o fonte do MIXR antes
// de escrever uma linha (contexts/src/mixr/):
//
//  1) weaponDynamics(dt) -- despacha por estagio. FREEFALL usa a fisica
//     nativa do Effect (arrasto+gravidade, ver o .cpp para a conta da
//     terminal); CANOPY/LANDED zeram a velocidade horizontal e fixam a
//     vertical (taxa constante ou zero), sem deriva de vento -- limitacao
//     documentada, nao bug.
//  2) updateTOF(dt) -- para de incrementar (e portanto nunca expira
//     'maxTOF') uma vez LANDED. Sem isto um paraquedista parado no chao
//     numa simulacao longa se autodetona ao vencer o TOF maximo -- o
//     mecanismo de TOF e' independente da AGL, ver AbstractWeapon.cpp.
//  3) dynamics(dt) -- o PONTO DE SAIDA: enquanto PRE_RELEASE, fixa o offset
//     de nascimento em relacao a aeronave que lancou (15 m atras, 10 m
//     abaixo, por default). Ver o comentario da implementacao no .cpp para o
//     porque de ser aqui, e nao no ciclo de decisao.
//  4) crashNotification()/collisionNotification() -- se a AGL cruzar zero
//     antes do ciclo de decisao reagir (um frame de atraso e' possivel:
//     ver docs/ARCHITECTURE.md), o CRASH_EVENT generico do Player dispara.
//     'Effect::crashNotification()' IGNORA 'crashOverride' (ao contrario de
//     'AbstractWeapon::crashNotification()') e sempre detona
//     (killedNotification() -> KILL_EVENT + dano/fumaca/chamas = 1.0) --
//     semanticamente errado para um pouso seguro. Aqui, vira so' LANDED,
//     sem cascata nenhuma.
//------------------------------------------------------------------------------
class Paratrooper final : public mixr::models::Effect
{
   DECLARE_SUBCLASS(Paratrooper, mixr::models::Effect)

public:
   Paratrooper();

   domain::Stage getJumpStage() const   { return stage_; }
   void setJumpStage(domain::Stage);

   double getCanopyDescentRateMps() const { return canopyDescentRateMps_; }

   double getReleaseOffsetAftM() const   { return releaseOffsetAftM_; }
   double getReleaseOffsetBelowM() const { return releaseOffsetBelowM_; }

   const char* getDescription() const override;
   const char* getNickname() const override;

   bool crashNotification() override;
   bool collisionNotification(mixr::models::Player* const p) override;

protected:
   void dynamics(const double dt) override;
   void weaponDynamics(const double dt) override;
   void updateTOF(const double dt) override;

private:
   bool setSlotCanopyDescentRate(const base::Number* const);
   bool setSlotReleaseOffsetAft(const base::Distance* const);
   bool setSlotReleaseOffsetBelow(const base::Distance* const);

   domain::Stage stage_{domain::Stage::FREEFALL};
   double canopyDescentRateMps_{5.5};

   // Offset de nascimento, em eixos do CORPO da aeronave lancadora --
   // ver dynamics() no .cpp.
   double releaseOffsetAftM_{15.0};
   double releaseOffsetBelowM_{10.0};
};

} // namespace xparatrooper
} // namespace models
} // namespace mixr
