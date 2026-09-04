//
// A FRONTEIRA C do plugin -- mesmo molde de models/flight/src/plugin.cpp
// e models/fixtures/stub/src/stub.cpp. So um nome, entao a factory compara
// direto -- sem a indirecao de um xnative/factory.{hpp,cpp} separado, que so
// paga para si com mais de uma classe.
//
#include "xplugin/PluginAbi.hpp"

// Canario sizeof(models::Player) que a macro grava no descritor.
#include "mixr/models/player/Player.hpp"

#include "xmissile/GuidedMissile.hpp"

#include <cstring>

namespace {

mixr::base::Object* fabrica(const char* const name)
{
   if (name == nullptr) return nullptr;
   if (std::strcmp(name, "GuidedMissile") == 0) return new mixr::xmissile::GuidedMissile();
   return nullptr;
}

const char* const NOMES[] = { "GuidedMissile", nullptr };

const mixr::base::MetaObject* const METAS[] = {
   mixr::xmissile::GuidedMissile::getMetaObject(),
   nullptr
};

} // namespace

MIXR_PLUGIN_DEFINE("missile", fabrica, NOMES, METAS)
