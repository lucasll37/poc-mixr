#include "poc/factory.hpp"
#include "poc/MyStation.hpp"
#include "poc/MySimulation.hpp"
#include "poc/MyAircraft.hpp"
#include "poc/MyDynamics.hpp"
#include "poc/MyNavigation.hpp"
#include "poc/MyRadar.hpp"

#include "mixr/base/factory.hpp"
#include "mixr/simulation/factory.hpp"
#include "mixr/models/factory.hpp"

namespace poc {

mixr::base::Object* factory(const std::string& name)
{
    mixr::base::Object* obj {};

    if      (name == MyStation::getFactoryName())    obj = new MyStation();
    else if (name == MySimulation::getFactoryName()) obj = new MySimulation();
    else if (name == MyAircraft::getFactoryName())   obj = new MyAircraft();
    else if (name == MyDynamics::getFactoryName())   obj = new MyDynamics();
    else if (name == MyNavigation::getFactoryName()) obj = new MyNavigation();
    else if (name == MyRadar::getFactoryName())      obj = new MyRadar();

    if (obj == nullptr) obj = mixr::models::factory(name);
    if (obj == nullptr) obj = mixr::simulation::factory(name);
    if (obj == nullptr) obj = mixr::base::factory(name);

    return obj;
}

} // namespace poc
