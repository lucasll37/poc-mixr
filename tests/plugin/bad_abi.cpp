//
// PLUGIN DEFEITUOSO DE PROPOSITO -- so para teste.
//
// Descritor montado A MAO (a macro nao serve: ela grava o PLUGIN_ABI certo)
// com a versao do contrato deslocada. Simula um plugin compilado contra um SDK
// mais antigo ou mais novo do que o host.
//
// O host tem de recusar imprimindo OS DOIS numeros -- e nao carregar e torcer.
//
#include "xplugin/PluginAbi.hpp"
#include "mixr/base/Object.hpp"

namespace {
mixr::base::Object* fabrica(const char*) { return nullptr; }
const char* const NAMES[] = { "NuncaCarrega", nullptr };
}

extern "C" MIXR_PLUGIN_EXPORT const mixr::xplugin::PluginDescV1* mixr_plugin_v1(void)
{
   static const mixr::xplugin::PluginDescV1 desc{
      sizeof(mixr::xplugin::PluginDescV1),
      mixr::xplugin::PLUGIN_ABI + 99,          // <- o defeito
      MIXR_VERSION, MIXR_PLUGIN_CXX11_ABI, 0, __cplusplus / 100,
      "bad-abi", MIXR_PLUGIN_PKG_VERSION, MIXR_PLUGIN_BUILD_ID,
      NAMES, nullptr, &fabrica
   };
   return &desc;
}
