#pragma once

//------------------------------------------------------------------------------
// MIXR_DEFAULT_VISIBILITY -- alvo compilado com gnu_symbol_visibility:'hidden'
// tem TODOS os simbolos escondidos por padrao, inclusive a PROPRIA API
// publica da lib: sem esta marca explicita em cada declaracao que precisa ser
// vista de fora, o consumidor falha no link ('nm -D' na .so devolve ZERO
// simbolos fortes -- o sintoma so aparece se alguem for olhar). Usada por
// toda shared_library() deste repositorio que compila hidden hoje --
// libs/xinfer (XINFER_API), libs/xpyembed (XPYEMBED_API) e o ponto de
// entrada de libs/xplugin (MIXR_PLUGIN_EXPORT, que acrescenta 'used' por
// cima, ver o comentario dela em PluginAbi.hpp).
//
// Esta macro e o comentario acima existiam duplicados, quase identicos, em
// tres libs diferentes (libs/xinfer, libs/xpyembed, libs/xplugin) antes de
// serem consolidados aqui -- as tres ja tem libs/ em
// include_directories('..'), entao nenhuma precisa de dependencia nova para
// alcancar este header.
//------------------------------------------------------------------------------
#define MIXR_DEFAULT_VISIBILITY __attribute__((visibility("default")))
