#pragma once

#include <string>
#include <vector>

namespace app {

//------------------------------------------------------------------------------
// A parte SEM FTXUI do destaque de sintaxe .edl usado na aba "EDL" (F7, ver
// app/src/app/DashboardLoop.cpp) -- a MESMA gramatica e a MESMA ordem de
// precedencia de 'EDL_TOKEN_RE'/'tokenizeEdlText()' em
// src/ui/edl_builder_core.js, o tokenizador que ja colore a
// "Pre-visualizacao .edl" do editor grafico web (src/ui/edl-builder.html).
// Porta pura -- so' string, sem FTXUI/MIXR -- pelo mesmo motivo de
// app/EdlEditorState.hpp: testavel sem levantar tela nenhuma
// (tests/app/test_edl_syntax_highlight.cpp espelha, ponto a ponto, a
// bateria em src/ui/edl_builder.test.js).
//------------------------------------------------------------------------------

// Categoria de um token -- os mesmos "tok-*" do lado JS. 'Plain' e' o unico
// sem estilo (espaco em branco, pontuacao nao reconhecida, um '(' sem
// classe atras, etc.).
enum class EdlTokenKind {
   Plain,
   Comment,
   String,
   ClassName,
   SlotName,
   Bool,
   Num,
   Punct,
   Value,
};

struct EdlToken {
   std::string text;
   EdlTokenKind kind{EdlTokenKind::Plain};
};

// Tokeniza 'text' na MESMA gramatica/precedencia de EDL_TOKEN_RE -- a ORDEM
// entre as alternativas importa (ex.: "true:" vira slot "true" + ":", nao o
// booleano "true", porque a forma "identificador+':'" e' tentada antes da
// forma "true|TRUE|false|FALSE"). Concatenar 'text' de todo token, na
// ordem, reproduz o texto de entrada byte a byte -- mesmo invariante de
// round-trip ja garantido do lado JS: o destaque nunca CORROMPE o que e'
// mostrado, so' colore por cima.
std::vector<EdlToken> tokenizeEdlText(const std::string& text);

} // namespace app
