//
// PLUGIN DEFEITUOSO DE PROPOSITO -- so para teste.
//
// Declara o nome de fabrica "Aircraft", que o framework ja constroi.
//
// Como o registro de plugins e consultado por ULTIMO na cadeia, este nome
// nunca chegaria ao plugin: ele ficaria silenciosamente INERTE, e o autor
// passaria a tarde procurando por que a classe dele "nao e usada" -- a
// patologia de contexts/BTCPP-CONTEXT.md:2358.
//
// Por isso a ordem na cadeia nao e a defesa: a defesa e a sonda de colisao do
// PluginRegistry, que pergunta a mixrFactoryBuiltin() antes de registrar e
// recusa a carga nomeando os dois donos.
//
#include "xplugin/PluginAbi.hpp"
#include "mixr/base/Object.hpp"

namespace {
mixr::base::Object* fabrica(const char*) { return nullptr; }
const char* const NAMES[] = { "Aircraft", nullptr };   // <- o defeito
}

extern "C" MIXR_PLUGIN_EXPORT const mixr::xplugin::PluginDescV1* mixr_plugin_v1(void)
{
   static const mixr::xplugin::PluginDescV1 desc{
      sizeof(mixr::xplugin::PluginDescV1), mixr::xplugin::PLUGIN_ABI,
      MIXR_VERSION, MIXR_PLUGIN_CXX11_ABI, 0, __cplusplus / 100,
      "bad-collide", MIXR_PLUGIN_PKG_VERSION, MIXR_PLUGIN_BUILD_ID,
      NAMES, nullptr, &fabrica
   };
   return &desc;
}
