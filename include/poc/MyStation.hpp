#pragma once
#include "mixr/simulation/Station.hpp"

namespace poc {

class MyStation final : public mixr::simulation::Station
{
    DECLARE_SUBCLASS(MyStation, mixr::simulation::Station)
public:
    MyStation();
    static const char* getFactoryName() { return "MyStation"; }
};

} // namespace poc
