//
// A FRONTEIRA C do MODELO -- e so isso.
//
// Todo o resto (domain/, ubf/, xnative/) e C++ comum e nao sabe que esta
// dentro de um plugin. Este arquivo existe so para emitir os simbolos que o
// dlsym procura, e ele nao escreve nenhuma assinatura a mao: quem escreve e
// a macro MIXR_PLUGIN_DEFINE.
//
// Isso e deliberado. A armadilha classica dos plugins, registrada em
// contexts/BTCPP-CONTEXT.md, e a funcao de registro acabar 'static' e ficar
// invisivel ao dlsym -- com o sintoma aparecendo longe, como "tipo nao
// registrado". Sem assinatura escrita a mao, nao ha onde enfiar um
// 'static'.
//
#include "xplugin/PluginAbi.hpp"

// Necessario para o canario sizeof(models::Player) que a macro grava no
// descritor -- e o que pega "mexeram no Player e esqueceram de recompilar o
// modelo". Ver a secao Limites de libs/xplugin/README.md para o que ele
// NAO pega.
#include "mixr/models/player/Player.hpp"

#include "xnative/factory.hpp"

#include <string>

namespace {

// O contrato atravessa a fronteira com 'const char*', nao
// 'const std::string&' (ver mixr::xplugin::factory_fn). A factory do modelo
// continua com a assinatura do resto do repo; a adaptacao e esta linha.
mixr::base::Object* fabrica(const char* const name)
{
   return (name != nullptr) ? mixr::models::xaaa::factory(std::string{name}) : nullptr;
}

} // namespace

// O primeiro argumento vai para o descritor binario do plugin e aparece em
// mensagens de erro do host quando algo da errado no carregamento -- ver
// PluginDescV1 em libs/xplugin/PluginAbi.hpp. Combina com o nome do
// project() em meson.build e do shared_module() (ambos "aaa").
MIXR_PLUGIN_DEFINE("aaa",
                   fabrica,
                   mixr::models::xaaa::factoryNames(),
                   mixr::models::xaaa::metaObjects())
