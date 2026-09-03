#include "mixr_factory.hpp"

#include "xplugin/PluginRegistry.hpp"
#include "xplugin/factory.hpp"

#include "mixr/simulation/factory.hpp"
#include "mixr/models/factory.hpp"
#include "mixr/terrain/factory.hpp"
#include "mixr/base/factory.hpp"

mixr::base::Object* mixrFactoryBuiltin(const std::string& name)
{
   mixr::base::Object* obj{};

   // 0) classes EDL da carga dinamica: ( PluginLoader ) e ( PluginModule ).
   if (obj == nullptr) obj = mixr::xplugin::factory(name);

   // O MODELO (flight_tc) chega pelo ULTIMO elo de mixrFactory(), via
   // mixr::xplugin::loadedFactory() -- dlopen durante o parse do .epp.

   // 1) framework -- Aircraft, JSBSimModel, Autopilot, Gimbal, Antenna, Tws,
   //    AirTrkMgr, SensorMgr, OnboardComputer, SigSphere...
   if (obj == nullptr) obj = mixr::simulation::factory(name);
   if (obj == nullptr) obj = mixr::models::factory(name);

   // 2) banco de elevacao: SrtmHgtFile. models::factory NAO encadeia esta --
   //    sem a linha abaixo, o 'terrain: ( SrtmHgtFile ... )' do .epp nao
   //    constroi nada e o WorldModel fica sem terreno, em silencio.
   if (obj == nullptr) obj = mixr::terrain::factory(name);

   if (obj == nullptr) obj = mixr::base::factory(name);

   return obj;
}

//------------------------------------------------------------------------------
// A factory que vai para o edl_parser. NUNCA devolve nullptr -- ver o
// cabecalho de mixr_factory.hpp para o porque (o parser deste fork nao
// reporta nome de fabrica desconhecido e devolver nullptr termina em SIGSEGV
// no edl_parser.y:179).
//------------------------------------------------------------------------------
mixr::base::Object* mixrFactory(const std::string& name)
{
   mixr::base::Object* obj{mixrFactoryBuiltin(name)};

   // Classes vindas do modelo carregado em tempo de execucao (dlopen), POR
   // ULTIMO de proposito -- ver o comentario identico em
   // src/poc/single-thread/src/mixr_factory.cpp.
   if (obj == nullptr) obj = mixr::xplugin::loadedFactory(name);

   if (obj == nullptr) mixr::xplugin::reportUnknownFactoryName(name);  // [[noreturn]]

   return obj;
}
