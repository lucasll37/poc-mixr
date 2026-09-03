#include "mixr_factory.hpp"

#include "xplugin/PluginRegistry.hpp"
#include "xplugin/factory.hpp"

#include "xtacview/factory.hpp"

#include "mixr/simulation/factory.hpp"
#include "mixr/models/factory.hpp"
#include "mixr/terrain/factory.hpp"
#include "mixr/recorder/factory.hpp"
#include "mixr/base/factory.hpp"

mixr::base::Object* mixrFactoryBuiltin(const std::string& name)
{
   mixr::base::Object* obj{};

   // 0) classes EDL da carga dinamica: ( PluginLoader ) e ( PluginModule ).
   if (obj == nullptr) obj = mixr::xplugin::factory(name);

   // O MODELO (domain/, bt/, ubf/, xnative/ -- incluindo RLBridgeBehavior)
   // nao entra aqui: chega pelo ULTIMO elo de mixrFactory(), via
   // mixr::xplugin::loadedFactory(), depois do dlopen do 'libflight_tc.so'
   // declarado no .epp.

   // 2) exportacao para o Tacview (opcional, so para acompanhar visualmente)
   if (obj == nullptr) obj = mixr::xtacview::factory(name);

   // 3) framework -- Aircraft, JSBSimModel, Autopilot, Gimbal, Antenna, Tws,
   //    AirTrkMgr, SensorMgr, OnboardComputer, FlightAgentTC, UbfArbiter,
   //    SigSphere, DataRecorder...
   if (obj == nullptr) obj = mixr::simulation::factory(name);
   if (obj == nullptr) obj = mixr::models::factory(name);

   // 4) banco de elevacao: SrtmHgtFile. models::factory NAO encadeia esta --
   //    sem a linha abaixo, 'terrain: ( SrtmHgtFile ... )' nao constroi nada,
   //    em silencio (mesma armadilha documentada nas outras pocs).
   if (obj == nullptr) obj = mixr::terrain::factory(name);

   if (obj == nullptr) obj = mixr::recorder::factory(name);
   if (obj == nullptr) obj = mixr::base::factory(name);

   return obj;
}

mixr::base::Object* mixrFactory(const std::string& name)
{
   mixr::base::Object* obj{mixrFactoryBuiltin(name)};

   // Classes vindas do plugin do modelo, carregado por dlopen em tempo de
   // execucao -- POR ULTIMO, para so ACRESCENTAR nomes, nunca sombrear em
   // silencio uma classe que ja funciona (ver o mesmo raciocinio em
   // app/src/mixr_factory.cpp).
   if (obj == nullptr) obj = mixr::xplugin::loadedFactory(name);

   if (obj == nullptr) mixr::xplugin::reportUnknownFactoryName(name);  // [[noreturn]]

   return obj;
}
