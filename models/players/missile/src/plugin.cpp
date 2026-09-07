//
// A FRONTEIRA C do plugin -- mesmo molde de models/players/A-4/src/plugin.cpp
// e models/players/fixtures/stub/src/stub.cpp. So dois nomes, entao a factory compara
// direto -- sem a indirecao de um xnative/factory.{hpp,cpp} separado, que so
// paga para si com bem mais de duas classes.
//
#include "xplugin/PluginAbi.hpp"

// Canario sizeof(models::Player) que a macro grava no descritor.
#include "mixr/models/player/Player.hpp"

#include "xmissile/GuidedMissile.hpp"
#include "xmissile/MissileThreadTagProbe.hpp"

#include <cstring>

namespace {

mixr::base::Object* fabrica(const char* const name)
{
   if (name == nullptr) return nullptr;
   if (std::strcmp(name, "GuidedMissile") == 0)          return new mixr::models::xmissile::GuidedMissile();
   if (std::strcmp(name, "MissileThreadTagProbe") == 0)  return new mixr::models::xmissile::MissileThreadTagProbe();
   return nullptr;
}

const char* const NOMES[] = { "GuidedMissile", "MissileThreadTagProbe", nullptr };

const mixr::base::MetaObject* const METAS[] = {
   mixr::models::xmissile::GuidedMissile::getMetaObject(),
   mixr::models::xmissile::MissileThreadTagProbe::getMetaObject(),
   nullptr
};

} // namespace

MIXR_PLUGIN_DEFINE("missile", fabrica, NOMES, METAS)
