#include "poc/MyAircraft.hpp"
#include "mixr/models/navigation/Navigation.hpp"
#include "mixr/models/navigation/Steerpoint.hpp"

#include <iostream>
#include <iomanip>

namespace poc {

IMPLEMENT_SUBCLASS(MyAircraft, "MyAircraft")
EMPTY_SLOTTABLE(MyAircraft)
EMPTY_DELETEDATA(MyAircraft)

MyAircraft::MyAircraft() { STANDARD_CONSTRUCTOR() }

void MyAircraft::copyData(const MyAircraft& org, const bool cc)
{
    BaseClass::copyData(org);
    if (cc) reportTimer = 0.0;
    reportTimer = org.reportTimer;
}

void MyAircraft::updateTC(const double dt)
{
    // Relatório periódico no console — fase 0 apenas
    const auto* wm = getWorldModel();
    if (wm != nullptr && wm->phase() == 0) {
        reportTimer -= dt * 4.0;
        if (reportTimer <= 0.0) {
            reportTimer = REPORT_INTERVAL;

            const auto* nav  = getNavigation();
            const auto* stpt = (nav != nullptr) ? nav->getSteerpoint() : nullptr;
            const char* wp   = (stpt != nullptr) ? stpt->getName() : "---";

            std::cout
                << std::fixed << std::setprecision(2)
                << "[" << getName() << "]"
                << "  lat="   << getLatitude()   << "°"
                << "  lon="   << getLongitude()  << "°"
                << "  alt="   << getAltitudeFt() << "ft"
                << "  hdg="   << getHeadingD()   << "°"
                << "  spd="   << getVelocityKts()<< "kt"
                << "  mach="  << getMach()
                << "  aoa="   << getAngleOfAttackD() << "°"
                << "  g="     << getGload()
                << "  wp→"    << wp
                << "\n";
        }
    }
    BaseClass::updateTC(dt);
}

void MyAircraft::updateData(const double dt) { BaseClass::updateData(dt); }

} // namespace poc
