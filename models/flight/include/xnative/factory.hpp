#ifndef __xnative_factory_H__
#define __xnative_factory_H__

#include <string>

namespace mixr {
namespace base { class Object; class MetaObject; }
namespace xnative {

// Factory das classes MIXR deste MODELO.
//
// Ela nao e mais encadeada no mixrFactory() do executavel: o modelo virou um
// plugin, carregado com dlopen durante o parse do cenario. Quem a alcanca e
// src/<poc>/src/plugin.cpp, atraves do contrato de shared/xplugin/PluginAbi.hpp.
base::Object* factory(const std::string& name);

// Os nomes de fabrica que este modelo responde, terminado em nullptr.
//
// O registro de plugins usa isto ANTES de construir qualquer coisa: para
// recusar a carga se um nome ja existir na cadeia do host, e para conferir o
// 'provides:' declarado no .epp.
const char* const* factoryNames();

// Os MetaObject das classes deste modelo, terminado em nullptr.
//
// Sem isto, app/MetaObjectReport ficaria cego para o modelo inteiro --
// reportClass<T>() e template (precisa do tipo em tempo de compilacao) e
// getMetaObject() e estatica, nao virtual (macros.hpp:136), entao de um
// Object* nao ha caminho ate o MetaObject. E a camada de teste de vazamento
// perderia justamente o codigo que mais muda.
const base::MetaObject* const* metaObjects();

}
}

#endif
