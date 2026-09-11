#pragma once

#include <string>

namespace mixr {
namespace base { class Object; class MetaObject; }
namespace models {
namespace xaaa {

// Factory das classes MIXR deste modelo.
//
// Nao e encadeada no mixrFactory() do executavel do host -- o modelo e um
// plugin, carregado com dlopen durante o parse do cenario. Quem a alcanca e
// src/plugin.cpp, atraves do contrato de libs/xplugin/PluginAbi.hpp. Ver
// models/players/A-4/include/xnative/factory.hpp para o mesmo desenho aplicado a
// um modelo com muito mais classes.
base::Object* factory(const std::string& name);

// Os nomes de fabrica que este modelo responde, terminado em nullptr.
//
// O registro de plugins usa isto ANTES de construir qualquer coisa: para
// recusar a carga se um nome ja existir na cadeia do host, e para conferir
// o 'provides:' declarado no .edl do cenario.
const char* const* factoryNames();

// Os MetaObject das classes deste modelo, terminado em nullptr.
//
// Sem isto, um relatorio de metaobjetos do host (ex.: a aba Memoria do
// ./app) ficaria cego para o modelo inteiro -- reportClass<T>() e template
// (precisa do tipo em tempo de compilacao) e getMetaObject() e estatica,
// nao virtual, entao de um Object* nao ha caminho ate o MetaObject.
const base::MetaObject* const* metaObjects();

} // namespace xaaa
} // namespace models
} // namespace mixr
