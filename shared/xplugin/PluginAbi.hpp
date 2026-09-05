#ifndef __xplugin_PluginAbi_H__
#define __xplugin_PluginAbi_H__

#include <cstdint>

// Unico include de MIXR neste header: traz MIXR_VERSION, que viaja no
// descritor. Nao puxa Object.hpp de proposito -- so precisamos do nome do
// tipo, nao da definicao (ver a declaracao adiantada abaixo).
#include "mixr/config.hpp"

//------------------------------------------------------------------------------
// O CONTRATO entre a aplicacao (host) e um modelo carregado em tempo de
// execucao (dlopen).
//
// Este e o UNICO arquivo que o autor de um plugin inclui do lado da
// aplicacao, e ele e HEADER-ONLY de proposito. As cinco bibliotecas de
// shared/ (xtacview, xclock, xjoystick, xlog, xmsg) sao static_library(): um
// plugin que linkasse a lib do registro ganharia a SUA PROPRIA copia do
// registro -- a aplicacao registraria de um lado e consultaria do outro.
// E a armadilha registrada em contexts/BTCPP-CONTEXT.md:7262-7270.
//
// REGRA DERIVADA, valida para todo plugin:
//    dependencies : [mixr_dep, xplugin_abi_dep]
// e NUNCA xlog_dep / xmsg_dep / xtacview_dep / xclock_dep / xjoystick_dep.
//
// Isso e reforcado pelo build: o executavel nao exporta simbolo nenhum e
// todo alvo compartilhado leva -Wl,--no-undefined, entao um plugin que
// dependesse de codigo da aplicacao nao LINKA -- o erro chega em tempo de
// build, nao de dlopen.
//
// O QUE ESTE CONTRATO GARANTE: que o ponto de entrada existe, que o plugin
// concorda com a versao do contrato, com a ABI de std::string e com o
// sizeof(models::Player).
//
// O QUE ELE NAO GARANTE esta em shared/xplugin/README.md, secao "Limites".
// Leia antes de confiar: varias divergencias de ABI sao INDETECTAVEIS aqui.
//------------------------------------------------------------------------------

namespace mixr {
namespace base { class Object; class MetaObject; }
namespace xplugin {

//------------------------------------------------------------------------------
// Versao do CONTRATO desta aplicacao -- nao e a versao do MIXR.
//
// Suba A MAO quando PluginDescV1 mudar de forma incompativel ou quando a
// semantica de um campo mudar. Acrescentar campo NO FIM nao exige subir: o
// host compara struct_size e simplesmente nao le o que nao conhece.
//------------------------------------------------------------------------------
constexpr std::uint32_t PLUGIN_ABI{1};

// Sentinela para 'cxx11_abi' quando a toolchain nao define
// _GLIBCXX_USE_CXX11_ABI (libc++, por exemplo). O host trata como AVISO,
// nao como recusa -- nao da para conferir o que nao foi reportado.
constexpr std::uint32_t CXX11_ABI_DESCONHECIDA{255};

//------------------------------------------------------------------------------
// A fabrica do plugin.
//
// 'const char*' e nao 'const std::string&' (que e o que mixr::base::factory_func
// usa): tira um tipo da libstdc++ da NOSSA fronteira. Isso NAO elimina a
// dependencia de ABI -- os headers do MIXR ja a impoem, e o vetor concreto e
// MetaObject::getClassName(), que e inline sobre um std::string MEMBRO. Por
// isso o campo cxx11_abi abaixo continua sendo motivo de RECUSA, e nao de
// aviso. O ganho aqui e de legibilidade do contrato: nada da biblioteca
// padrao atravessa a fronteira que nos desenhamos.
//
// Semantica identica a das factories de shared/x<nome>: devolve objeto novo
// com refCount 1, ou nullptr se o nome nao e deste plugin.
//------------------------------------------------------------------------------
using factory_fn = ::mixr::base::Object* (*)(const char* name);

//------------------------------------------------------------------------------
// O descritor. POD, com struct_size na frente.
//
// 'struct_size' e o que torna campo aditivo seguro: um host novo lendo um
// plugin velho ve um numero menor e nao toca no que veio depois.
//------------------------------------------------------------------------------
struct PluginDescV1
{
   std::uint32_t struct_size;      // sizeof(PluginDescV1) COMO O PLUGIN VE
   std::uint32_t abi;              // PLUGIN_ABI com que o plugin foi compilado
   std::uint32_t mixr_version;     // MIXR_VERSION do config.hpp que o plugin viu
   std::uint32_t cxx11_abi;        // _GLIBCXX_USE_CXX11_ABI (255 = desconhecida)
   std::uint32_t player_size;      // sizeof(models::Player) -- CANARIO (0 = opt-out)
   std::uint32_t cxx_standard;     // __cplusplus/100 -- SO DIAGNOSTICO, nunca recusa

   const char* plugin_name;        // nome legivel, para log
   const char* mixr_pkg_version;   // "1.0.5" (mixr_dep.version()) -- AVISO
   const char* build_id;           // "gcc 13.3.0" -- SO DIAGNOSTICO

   // Terminado em nullptr. Obrigatorio: e o que permite detectar colisao de
   // nome ANTES de construir qualquer coisa, e conferir o 'provides:' do .edl.
   const char* const* factory_names;

   // Terminado em nullptr; pode ser nullptr inteiro. Existe por um motivo
   // concreto: app/MetaObjectReport usa um template reportClass<T>() -- COMPILE
   // TIME -- e por construcao nao enxerga classe que o executavel nao conhece
   // (getMetaObject() e estatica, nao virtual). Sem isto, a camada de teste de
   // vazamento ficaria cega justamente para o codigo mais novo do processo.
   const ::mixr::base::MetaObject* const* metas;

   factory_fn factory;
};

} // namespace xplugin
} // namespace mixr

//------------------------------------------------------------------------------
// O ponto de entrada.
//
// Nome FIXO (dlsym recebe uma string) e extern "C" (sem isso o simbolo seria
// _Z15mixr_plugin_v1v, e o mangling mudaria se alguem renomeasse o namespace
// ou movesse a struct).
//
// O '_v1' NO NOME e redundante com o campo 'abi' DE PROPOSITO -- sao dois
// tipos diferentes de mudanca:
//   * campo novo no fim (ADITIVA)   -> coberto por struct_size + abi;
//   * mudanca DESTRUTIVA            -> coberta pelo nome: o host novo procura
//     mixr_plugin_v2, o plugin velho so exporta mixr_plugin_v1, e o dlsym
//     falha com "simbolo ausente" em vez de reinterpretar bytes.
//------------------------------------------------------------------------------
extern "C" const ::mixr::xplugin::PluginDescV1* mixr_plugin_v1(void);

//------------------------------------------------------------------------------
// visibility("default") porque o alvo do plugin compila com
// gnu_symbol_visibility:'hidden' -- sem isto o unico simbolo que importa some
// junto com os outros, e o sintoma seria "simbolo ausente" no dlsym.
//
// 'used' impede que um build futuro com LTO ou --gc-sections descarte a
// funcao (hoje b_lto=false, mas isso e um default, nao uma garantia).
//------------------------------------------------------------------------------
#define MIXR_PLUGIN_EXPORT __attribute__((visibility("default"), used))

#if defined(_GLIBCXX_USE_CXX11_ABI)
   #define MIXR_PLUGIN_CXX11_ABI (_GLIBCXX_USE_CXX11_ABI)
#else
   #define MIXR_PLUGIN_CXX11_ABI (::mixr::xplugin::CXX11_ABI_DESCONHECIDA)
#endif

// Injetados pelo meson (xplugin_abi_dep). Os defaults existem para que um
// plugin compilado a mao ainda compile -- e o host avisa que nao sabe contra
// o que ele foi feito.
#ifndef MIXR_PLUGIN_PKG_VERSION
   #define MIXR_PLUGIN_PKG_VERSION "?"
#endif
#ifndef MIXR_PLUGIN_BUILD_ID
   #define MIXR_PLUGIN_BUILD_ID "?"
#endif

//------------------------------------------------------------------------------
// A macro que emite o ponto de entrada inteiro.
//
// USE SEMPRE ESTA MACRO. Ela existe para fechar a armadilha classica dos
// plugins, registrada em contexts/BTCPP-CONTEXT.md:7248: a macro
// BT_REGISTER_NODES do BehaviorTree.CPP expande para uma funcao 'static',
// invisivel ao dlsym -- e o sintoma aparece longe, como "tipo nao
// registrado", com o autor jurando que escreveu a funcao. Aqui o autor nao
// escreve a assinatura, entao nao ha onde enfiar um 'static'.
//
// Uso -- o exemplo minimo completo e models/player/fixtures/stub/src/stub.cpp, e o de
// producao e models/player/A4/src/plugin.cpp:
//
//    namespace {
//    const char* const NAMES[] = { "MinhaClasse", nullptr };
//    const mixr::base::MetaObject* const METAS[] = {
//       mixr::xmeu::MinhaClasse::getMetaObject(), nullptr };
//    }
//    MIXR_PLUGIN_DEFINE("meu-modelo", mixr::xmeu::factory, NAMES, METAS)
//
// O que um modelo TEM de fazer alem disto (as bases obrigatorias, os slots que
// o cenario nomeia e o dever de escrever no xboard) esta em
// models/player/fixtures/stub/docs/CONTRATO.md -- o descritor abaixo e so o EMPACOTAMENTO.
//------------------------------------------------------------------------------
#define MIXR_PLUGIN_DEFINE_IMPL(NAME, FACTORY_FN, NAMES, METAS, PLAYER_SZ)      \
   extern "C" MIXR_PLUGIN_EXPORT                                                \
   const ::mixr::xplugin::PluginDescV1* mixr_plugin_v1(void)                    \
   {                                                                            \
      static const ::mixr::xplugin::PluginDescV1 desc{                          \
         static_cast<std::uint32_t>(sizeof(::mixr::xplugin::PluginDescV1)),     \
         ::mixr::xplugin::PLUGIN_ABI,                                           \
         static_cast<std::uint32_t>(MIXR_VERSION),                              \
         static_cast<std::uint32_t>(MIXR_PLUGIN_CXX11_ABI),                     \
         static_cast<std::uint32_t>(PLAYER_SZ),                                 \
         static_cast<std::uint32_t>(__cplusplus / 100),                         \
         (NAME),                                                                \
         MIXR_PLUGIN_PKG_VERSION,                                               \
         MIXR_PLUGIN_BUILD_ID,                                                  \
         (NAMES),                                                               \
         (METAS),                                                               \
         (FACTORY_FN)                                                           \
      };                                                                        \
      return &desc;                                                             \
   }

// Versao normal. Exige models/player/Player.hpp incluido no ponto de
// expansao -- de proposito: o canario de sizeof so faz sentido para quem
// deriva de Player, e quem deriva ja incluiu.
#define MIXR_PLUGIN_DEFINE(NAME, FACTORY_FN, NAMES, METAS)                      \
   MIXR_PLUGIN_DEFINE_IMPL(NAME, FACTORY_FN, NAMES, METAS,                      \
                           sizeof(::mixr::models::Player))

// Para plugin que NAO deriva de models::Player (um OutputHandler, um
// Behavior do UBF...). Desliga o canario; o host avisa que a checagem mais
// util de layout esta desligada.
#define MIXR_PLUGIN_DEFINE_NOCANARY(NAME, FACTORY_FN, NAMES, METAS)             \
   MIXR_PLUGIN_DEFINE_IMPL(NAME, FACTORY_FN, NAMES, METAS, 0)

#endif
