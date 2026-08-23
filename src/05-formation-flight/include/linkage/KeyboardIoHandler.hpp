#pragma once

#include "mixr/linkage/IoHandler.hpp"

#include "formations.hpp"

#include <string>

namespace mixr { namespace terrain { class Terrain; } }

namespace keyboard {

//------------------------------------------------------------------------------
// Class: KeyboardIoHandler
//
// Le os canais discretos do KeyboardDevice (via readDeviceInputs() nativo
// do IoHandler) e traduz em comandos: heading/altitude/velocidade do
// Autopilot do lider (mecanismo nativo de comando do Autopilot), troca de
// formacao e RTB (via FormationState compartilhado com os wingmen -- ver
// formations.hpp -- e ativando o navMode do Autopilot do lider, que usa a
// Route/Steerpoints ja declarados no seu Navigation). Mesmo padrao de
// examples/mainSim2/SimIoHandler.cpp: acha o Station/ownship subindo a
// arvore de containers com findContainerByType().
//
// Factory name: KeyboardIoHandler
//------------------------------------------------------------------------------
class KeyboardIoHandler final : public mixr::linkage::IoHandler
{
   DECLARE_SUBCLASS(KeyboardIoHandler, mixr::linkage::IoHandler)

public:
   KeyboardIoHandler();

   // injetado pelo main.cpp logo apos o parse do EDL (nao e um objeto
   // MIXR, so um ponteiro pro estado compartilhado com os BTs dos wingmen)
   void setFormationState(formations::FormationState* const fs) { formationState = fs; }
   void setTerrain(const mixr::terrain::Terrain* const t) { terrain = t; }

   // consumido pelo main.cpp a cada tick p/ encaminhar ao Tacview/recorder
   std::string consumePendingEvent();

private:
   void inputDevicesImpl(const double dt) final;
   void outputDevicesImpl(const double dt) final { writeDeviceOutputs(dt); }

   void queueEvent(const std::string& text) { pendingEvent = text; }

   formations::FormationState* formationState{};
   const mixr::terrain::Terrain* terrain{};
   std::string pendingEvent;

   bool initialized{};
   double cmdHeadingDeg{};
   double cmdAltitudeFt{};
   double cmdVelocityKts{};

   static constexpr double kHeadingRateDps{15.0};   // por segundo de tecla segurada
   static constexpr double kAltitudeRateFpm{500.0};  // pes por minuto
   static constexpr double kVelocityRateKtps{10.0};  // nos por segundo
};

} // namespace keyboard
