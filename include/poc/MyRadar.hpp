#pragma once
#include "mixr/models/system/Radar.hpp"

namespace poc {

class MyRadar final : public mixr::models::Radar
{
    DECLARE_SUBCLASS(MyRadar, mixr::models::Radar)
public:
    MyRadar();
};

} // namespace poc
