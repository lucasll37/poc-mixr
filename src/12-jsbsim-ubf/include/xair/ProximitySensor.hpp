#ifndef __xair_ProximitySensor_H__
#define __xair_ProximitySensor_H__

#include "mixr/models/system/System.hpp"

#include <mutex>
#include <string>

namespace mixr {
namespace base { class Angle; class Distance; class Number; class Time; }

namespace xair {

//------------------------------------------------------------------------------
// Class: ProximitySensor
//
// Description: Sensor proprio, puramente geometrico: varre a lista de
//              players do WorldModel e reporta o contato mais proximo
//              dentro de um alcance e de um setor a frente. NAO e o radar
//              nativo (Antenna/Tws/AirTrkMgr das pocs 06/07) -- e o
//              "sensor de brinquedo" que existe justamente para mostrar
//              quanta coisa um subsistema proprio precisa fazer, e em que
//              fase.
//
// Factory name: ProximitySensor
//
// Slots:
//    maxRange     <Distance>  ! Alcance maximo (default: 8 NM)
//    fieldOfView  <Angle>     ! Meio-angulo do setor a frente (default: 70 deg)
//    scanInterval <Time>      ! Intervalo entre varreduras (default: 0.5 s)
//    holdTime     <Time>      ! Memoria da pista apos perder o contato (default: 4 s)
//    hostileOnly  <Number>    ! !=0 => so detecta players de outro 'side' (default: 1)
//
// 'holdTime' existe por um motivo achado RODANDO: sem memoria de pista, a
// manobra de evasao vira o drone, o intruso sai do setor a frente, o
// contato some no mesmo frame e a arvore volta para PATROL -- que vira de
// volta e reencontra o contato. O resultado e um EVADE/PATROL piscando a
// cada poucos frames. Segurar a ultima pista por alguns segundos e o que
// um sensor de verdade faz (e o que o AirTrkMgr nativo faz com a idade da
// pista) e resolve o problema no lugar certo: no sensor, nao na arvore.
//
// FASE: a varredura roda em receive() -- FASE 2 do frame ("sensores
// recebem"). E o que torna a leitura das posicoes dos OUTROS players segura
// mesmo com o pool multithread ligado: toda posicao foi escrita na fase 0 e
// nenhuma sera escrita de novo antes da fase 0 do proximo frame. A barreira
// entre fases e do proprio framework (Simulation espera todas as threads
// antes de trocar de fase), nao nossa.
//
// O contato e publicado sob mutex porque o laco de background do main.cpp
// (outra thread) le esse estado para imprimir o status.
//------------------------------------------------------------------------------
class ProximitySensor : public models::System
{
   DECLARE_SUBCLASS(ProximitySensor, models::System)

public:
   ProximitySensor();

   struct Contact
   {
      bool valid{};
      double rangeM{};
      double relBearingDeg{};
      double deltaAltM{};
      std::string name;
   };

   void reset() override;

   Contact getContact() const;
   bool hasContact() const;

   double getMaxRangeM() const     { return maxRangeM; }
   long getScanCount() const;

protected:
   // FASE 2 -- varredura
   void receive(const double dt) override;

private:
   double maxRangeM{8.0 * 1852.0};
   double fovDeg{70.0};
   double scanIntervalSec{0.5};
   double holdTimeSec{4.0};
   bool hostileOnly{true};

   double scanTimer{};

   mutable std::mutex contactMutex;
   Contact contact;
   double holdTimer{};
   long scanCount{};

   // slot table helpers
   bool setSlotMaxRange(const base::Distance* const);
   bool setSlotFieldOfView(const base::Angle* const);
   bool setSlotScanInterval(const base::Time* const);
   bool setSlotHoldTime(const base::Time* const);
   bool setSlotHostileOnly(const base::Number* const);
};

} // namespace xair
} // namespace mixr

#endif
