#pragma once
#include "mixr/models/WorldModel.hpp"

namespace poc {

class MySimulation final : public mixr::models::WorldModel
{
    DECLARE_SUBCLASS(MySimulation, mixr::models::WorldModel)
public:
    MySimulation();
};

} // namespace poc
