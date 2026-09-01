#ifndef __xplugin_PluginLoader_H__
#define __xplugin_PluginLoader_H__

#include "mixr/base/Component.hpp"

#include <string>
#include <vector>

namespace mixr {
namespace base { class PairStream; }
namespace xplugin {

//------------------------------------------------------------------------------
// O bloco do .epp que carrega modelos em tempo de execucao.
//
// Factory name: PluginLoader
//
// Slots:
//    searchPaths  <PairStream de String>       ! onde procurar, NA ORDEM
//    modules      <PairStream de PluginModule> ! o que carregar
//
// POR QUE DERIVA DE base::Component, e nao de base::Object: e o unico ponto
// de ancoragem que existe nas TRES pocs. O slot 'components:' vem de
// base::Component, entao existe tanto na ( ClockStation ) das gemeas quanto
// na ( Station ) de estoque do bandit-dis -- um slot novo so existiria numa
// Station nossa. Component::processComponents liga skipFilter quando o filtro
// e typeid(Component) (Component.cpp:595-598) e a Station nao sobrescreve o
// metodo, entao qualquer Component e aceito e mantido.
//
// Como Component este objeto e INERTE: nao sobrescreve updateTC, updateData,
// reset nem event.
//
//------------------------------------------------------------------------------
// A CARGA ACONTECE EM isValid(), E ESSE E O CORACAO DO DESENHO.
//
// A objecao obvia a declarar plugins no .epp e de ordem: o parser precisa da
// factory ANTES de construir o primeiro objeto. Ela nao se sustenta, e a
// razao e puramente sintatica. Da gramatica (edl_parser.y:143-174):
//
//    arglist :                        { $$ = new PairStream(); }
//            | arglist form           { ... }
//            | arglist prim           { ... }
//            | arglist slot_value     { $1->put($2); ... }
//            ;
//    form    : '(' IDENT arglist ')'  { $$ = parse($2, $3); ... }
//
// 'arglist' e RECURSIVA A ESQUERDA. Para reduzir 'arglist slot_value' o parser
// ja reduziu todo o arglist anterior, e cada slot_value exige o 'form' dele
// reduzido -- que e onde parse() roda. Logo:
//
//    * filhos antes dos pais       (aninhamento);
//    * IRMAOS NA ORDEM DO TEXTO    (recursao a esquerda).
//
// E parse() (edl_parser.y:70-103) faz, no fecha-parenteses de cada forma:
// factory(name) -> todos os setSlotByName -> isValid(). O proprio Object.hpp
// (109-112) documenta isValid() como "called by the parser to inform the
// object that all calls to setSlotByName() have been completed".
//
// Portanto um ( PluginLoader ) escrito ANTES de 'simulation:' tem o isValid()
// executado antes de qualquer player existir. Um parse, um arquivo, a factory
// de producao inteira -- sem passe descartavel e sem pre-scan de texto.
//
// Consequencias praticas:
//
//   * a ordem dos slots DENTRO do bloco nao importa (searchPaths pode vir
//     depois de modules) -- por isso a carga esta aqui, e nao num setter;
//   * mas a POSICAO do bloco no arquivo importa, e e a unica regra que o
//     autor do cenario precisa lembrar. Fora de ordem, o parser chega na
//     classe do plugin sem ninguem que responda por ela, e
//     PluginRegistry::reportUnknownFactoryName explica isso na mensagem;
//   * isValid() pode ser chamado mais de uma vez (Pair::isValid() repassa),
//     entao loadModule() e idempotente por caminho resolvido.
//
// Falha e FATAL, e nao 'return false'. Devolver false so incrementaria
// err_count e O PARSE CONTINUARIA -- chegando no player que usa a classe do
// plugin, a factory devolveria nulo e o edl_parser.y:179 faria
// $2->unref() sem checar nulo. Referenced::unref() e inline e mexe em
// refCount por offset fixo: SIGSEGV, nao erro. Morrer no ponto certo, com a
// causa na mao, e estritamente melhor.
//------------------------------------------------------------------------------
class PluginLoader : public base::Component
{
   DECLARE_SUBCLASS(PluginLoader, base::Component)

public:
   PluginLoader();

   bool isValid() const override;   // <- aqui mora o dlopen

private:
   std::vector<std::string> searchPaths_;
   base::PairStream* modules_{};

   bool setSlotSearchPaths(const base::PairStream* const);
   bool setSlotModules(base::PairStream* const);
};

} // namespace xplugin
} // namespace mixr

#endif
