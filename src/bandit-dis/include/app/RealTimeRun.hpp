#pragma once

namespace mixr {
namespace simulation { class Station; }
namespace linkage { class IoHandler; }
}

namespace app {

//------------------------------------------------------------------------------
// O laco de TEMPO REAL -- versao enxuta do padrao ja usado em
// single-thread/multi-thread (ver o comentario la para o porque geral).
// Aqui so ha tres coisas:
//
//   * joystick fisico do bandit1 (linkage::IoHandler::inputDevices()) --
//     com fallback automatico para o Autopilot quando nao ha hardware (ver
//     shared/xjoystick/JoystickIoHandler.hpp);
//   * station->updateData(dt) -- drena o gravador (Tacview) E processa o
//     'networks:' (o envio dos PDUs DIS acontece dentro dela, nao precisa
//     de nenhuma chamada a mais -- ver Station.hpp: interoperabilty
//     networks sao atualizadas dentro de updateData() quando 'netRate' nao
//     pede uma thread propria);
//   * o sleep que acerta o passo com o tempo de parede.
//
// Sem ClockStation/TimeControls, sem Fleet -- essa poc so tem um player.
//------------------------------------------------------------------------------
void runRealTime(mixr::simulation::Station* station, mixr::linkage::IoHandler* ioHandler);

} // namespace app
