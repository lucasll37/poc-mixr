#ifndef __xplugin_PluginRegistry_H__
#define __xplugin_PluginRegistry_H__

#include "mixr/base/edl_parser.hpp"   // mixr::base::factory_func

#include <string>
#include <vector>

namespace mixr {
namespace base { class Object; class MetaObject; }
namespace xplugin {

//------------------------------------------------------------------------------
// Registro de modelos carregados em tempo de execucao -- um por processo.
//
// Uma unica questao: quais .so estao carregadas, quais nomes de fabrica cada
// uma responde, e como construir por esses nomes. Nao decide QUANDO carregar
// (isso e do PluginLoader, no isValid() que o edl_parser chama) nem ONDE
// encadear (isso e do mixr_factory.cpp de cada subprojeto).
//
// O ESTADO E GLOBAL, E NAO HA ALTERNATIVA. O gancho de extensao do MIXR e
//    typedef Object* (*factory_func)(const std::string&);   // edl_parser.hpp:16
// -- ponteiro de funcao C nu, sem parametro de contexto, sem std::function.
// Uma lambda capturante nao passa por ali. Quem consulta este registro e o
// mixrFactory() de cada poc, que e funcao livre; logo o estado tem de estar
// em escopo de arquivo.
//
// CONCORRENCIA: escrito so durante o parse do .epp, na thread principal,
// antes de existir qualquer thread de tempo critico. Depois disso e so
// leitura. seal() marca o fim da janela de escrita e transforma uma carga
// tardia em erro, em vez de corrida silenciosa.
//
// NAO HA dlclose, NUNCA -- ver o cabecalho de PluginRegistry.cpp para o
// argumento completo (resumo: toda instancia viva guarda ponteiro para
// dentro da imagem do plugin, e o destrutor ESCREVE la).
//------------------------------------------------------------------------------

// A cadeia de fabricas que a aplicacao ja responde SEM plugin nenhum.
// Usada como SONDA: antes de registrar um nome, o registro pergunta se
// alguem ja constroi por ele, e recusa a carga se sim. Sem isso, um plugin
// que colidisse com o framework ficaria silenciosamente inerte -- a
// patologia registrada em contexts/BTCPP-CONTEXT.md:2358.
//
// Chamada uma vez, de app::buildStation(), imediatamente antes do edl_parser.
void setBuiltinFactory(base::factory_func);

// Carrega, valida e registra uma .so. TODA falha e fatal (std::exit) --
// ver a taxonomia no cabecalho de PluginRegistry.cpp para o porque.
//
// 'file' e resolvido contra 'searchPaths' na ordem; 'provides' (se nao
// vazio) e conferido contra o que a .so declara, e divergencia e fatal.
//
// Idempotente por caminho resolvido: isValid() pode ser chamado mais de uma
// vez para o mesmo objeto (Pair::isValid() repassa), e carregar duas vezes
// nao pode duplicar registro.
void loadModule(const std::string& file,
                const std::vector<std::string>& searchPaths,
                const std::vector<std::string>& provides);

// O elo encadeado no fim de mixr_factory.cpp. Devolve nullptr se nenhum
// plugin carregado responde por 'name'.
base::Object* loadedFactory(const std::string& name);

// Fecha a janela de escrita. Chamado depois do parse.
void seal();

// Os MetaObject que as .so carregadas exportaram (pode ser vazio). Consumido
// por app/MetaObjectReport, que sozinho nao alcanca classe de plugin:
// getMetaObject() e ESTATICA, nao virtual (macros.hpp:136), entao de um
// Object* nao ha caminho ate o MetaObject dele.
const std::vector<const base::MetaObject*>& pluginMetaObjects();

// Diagnostico terminal para nome de fabrica que ninguem constroi.
//
// Existe porque o edl_parser deste fork NAO reporta isso: a mensagem
// "undefined factory name" (edl_parser.y:97-100) esta num ramo alcancavel so
// com arg_list == nullptr, mas a producao 'arglist:' SEMPRE aloca um
// PairStream -- e codigo morto. E devolver nullptr ao parser leva ao
// edl_parser.y:179, que faz $2->unref() sem checar nulo; Referenced::unref()
// e inline e mexe em refCount por offset fixo, entao o resultado e SIGSEGV.
[[noreturn]] void reportUnknownFactoryName(const std::string& name);

} // namespace xplugin
} // namespace mixr

#endif
