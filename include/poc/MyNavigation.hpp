#pragma once
#include "mixr/models/navigation/Navigation.hpp"

namespace poc {

class MyNavigation final : public mixr::models::Navigation
{
    DECLARE_SUBCLASS(MyNavigation, mixr::models::Navigation)
public:
    MyNavigation();
};

} // namespace poc
