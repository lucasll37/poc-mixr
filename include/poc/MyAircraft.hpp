#pragma once
#include "mixr/models/player/air/AirVehicle.hpp"

namespace poc {

class MyAircraft final : public mixr::models::AirVehicle
{
    DECLARE_SUBCLASS(MyAircraft, mixr::models::AirVehicle)
public:
    MyAircraft();

    void updateTC(const double dt = 0.0) override;
    void updateData(const double dt = 0.0) override;

private:
    double reportTimer {};
    static constexpr double REPORT_INTERVAL { 5.0 };
};

} // namespace poc
