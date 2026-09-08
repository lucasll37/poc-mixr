//
// PLUGIN DEFEITUOSO DE PROPOSITO -- so para teste.
//
// Irmao de bad_collide.cpp, mas testa a OUTRA ramificacao da sonda de
// colisao (libs/xplugin/PluginRegistry.cpp:412-444): bad_collide.cpp declara
// um nome que o FRAMEWORK ja constroi ("Aircraft" -- builtinFactory_(n) !=
// nullptr, mensagem "JA e construido pelo framework"); este declara um nome
// que outro PLUGIN ja registrou ("FlightAction", do libflight.so de
// producao -- registry().find(n) != registry().end(), mensagem "ja foi
// registrado por <dono>"). As duas ramificacoes sao codigo DIFERENTE
// (PluginRegistry.cpp:416-430 contra 431-443) e so esta segunda cobre o
// cenario de um .so de terceiro colidindo com o modelo de producao, ou dois
// modelos de producao colidindo entre si -- o caso que check_colisao_fabrica.py
// (guarda ESTATICA, so compara nomes ENTRE modelos sob models/players/) nao
// alcanca: ela nunca chama loadModule() de verdade.
//
// Por isso este .so so faz sentido carregado JUNTO com libflight.so (ver
// run_plugin_negatives.py, caso "plugin colide com nome de OUTRO plugin ja
// carregado") -- sozinho ele carregaria sem erro nenhum.
//
#include "xplugin/PluginAbi.hpp"
#include "mixr/base/Object.hpp"

namespace {
mixr::base::Object* fabrica(const char*) { return nullptr; }
const char* const NAMES[] = { "FlightAction", nullptr };   // <- o defeito: nome ja usado por libflight.so
}

extern "C" MIXR_PLUGIN_EXPORT const mixr::xplugin::PluginDescV1* mixr_plugin_v1(void)
{
   static const mixr::xplugin::PluginDescV1 desc{
      sizeof(mixr::xplugin::PluginDescV1), mixr::xplugin::PLUGIN_ABI,
      MIXR_VERSION, MIXR_PLUGIN_CXX11_ABI, 0, __cplusplus / 100,
      "bad-collide-own-name", MIXR_PLUGIN_PKG_VERSION, MIXR_PLUGIN_BUILD_ID,
      NAMES, nullptr, &fabrica
   };
   return &desc;
}
