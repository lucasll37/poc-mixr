#ifndef __xjoystick_ChannelMap_H__
#define __xjoystick_ChannelMap_H__

namespace mixr {
namespace xjoystick {

//------------------------------------------------------------------------------
// Canais AI (analógicos) do IoData usados pelo JoystickIoHandler -- únicos
// entre o EDL (scenario.edl.in, "ai: ROLL_AI ...") e o C++
// (JoystickIoHandler.cpp) para não repetir os números em dois lugares.
//
// 1-based -- mesma convenção do mixr::linkage::IoData (ver IoData.hpp).
//------------------------------------------------------------------------------
constexpr int ROLL_AI{1};
constexpr int PITCH_AI{2};
constexpr int RUDDER_AI{3};
constexpr int THROTTLE_AI{4};

}
}

#endif
