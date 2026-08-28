#include "mixr_factory.hpp"

#include "xnative/factory.hpp"
#include "xtacview/factory.hpp"
#include "xclock/factory.hpp"

#include "mixr/simulation/factory.hpp"
#include "mixr/models/factory.hpp"
#include "mixr/terrain/factory.hpp"
#include "mixr/recorder/factory.hpp"
#include "mixr/base/factory.hpp"

mixr::base::Object* mixrFactory(const std::string& name)
{
   mixr::base::Object* obj{};

   // 1) o pouco que continua sendo nosso (UBF + datalink derivado)
   if (obj == nullptr) obj = mixr::xnative::factory(name);

   // 2) exportacao para o Tacview (shared/xtacview)
   if (obj == nullptr) obj = mixr::xtacview::factory(name);

   // 3) relogio: ClockStation (Station + acelerar/frear/pausar)
   if (obj == nullptr) obj = mixr::xclock::factory(name);

   // 4) framework -- daqui vem Aircraft, JSBSimModel, Autopilot, Gimbal,
   //    Antenna, Tws, AirTrkMgr, SensorMgr, OnboardComputer, SimAgent,
   //    UbfArbiter, SigSphere, DataRecorder...
   if (obj == nullptr) obj = mixr::simulation::factory(name);
   if (obj == nullptr) obj = mixr::models::factory(name);

   // 5) banco de elevacao: SrtmHgtFile / DtedFile / DedFile / QuadMap.
   //    models::factory NAO encadeia esta -- sem a linha abaixo, o
   //    'terrain: ( SrtmHgtFile ... )' do .epp nao constroi nada e o
   //    WorldModel fica sem terreno, em silencio.
   if (obj == nullptr) obj = mixr::terrain::factory(name);

   if (obj == nullptr) obj = mixr::recorder::factory(name);
   if (obj == nullptr) obj = mixr::base::factory(name);

   return obj;
}
