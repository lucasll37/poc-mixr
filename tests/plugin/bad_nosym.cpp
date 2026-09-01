//
// PLUGIN DEFEITUOSO DE PROPOSITO -- so para teste.
//
// Reproduz a armadilha de contexts/BTCPP-CONTEXT.md:7248: o ponto de entrada
// existe no fonte, mas nao chega ao .dynsym. Aqui a causa e omitir o
// MIXR_PLUGIN_EXPORT num alvo compilado com -fvisibility=hidden -- que e
// exatamente o que acontece com quem escreve a assinatura a mao em vez de usar
// a macro MIXR_PLUGIN_DEFINE.
//
// O host tem de recusar isto com uma mensagem nomeando o simbolo, e nao
// estourar depois.
//
#include "xplugin/PluginAbi.hpp"
#include "mixr/base/Object.hpp"

namespace {
mixr::base::Object* fabrica(const char*) { return nullptr; }
const char* const NAMES[] = { "NuncaCarrega", nullptr };
}

// SEM MIXR_PLUGIN_EXPORT -- e o defeito.
extern "C" const mixr::xplugin::PluginDescV1* mixr_plugin_v1(void)
{
   static const mixr::xplugin::PluginDescV1 desc{
      sizeof(mixr::xplugin::PluginDescV1), mixr::xplugin::PLUGIN_ABI,
      MIXR_VERSION, MIXR_PLUGIN_CXX11_ABI, 0, __cplusplus / 100,
      "bad-nosym", MIXR_PLUGIN_PKG_VERSION, MIXR_PLUGIN_BUILD_ID,
      NAMES, nullptr, &fabrica
   };
   return &desc;
}
