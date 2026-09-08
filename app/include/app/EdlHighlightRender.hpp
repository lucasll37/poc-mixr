#pragma once

#include "app/EdlSyntaxHighlight.hpp"

#include <ftxui/dom/elements.hpp>

#include <string>

//------------------------------------------------------------------------------
// A parte COM FTXUI do destaque de sintaxe .edl usado na aba "EDL" (F7, ver
// app/src/app/DashboardLoop.cpp) -- app/EdlSyntaxHighlight.hpp tokeniza (sem
// FTXUI); aqui e' so' cor + o corte do glifo sob o cursor.
//
// Extraido de dentro de um namespace anonimo de DashboardLoop.cpp (achado
// por auditoria: ficava INALCANCAVEL por teste nenhum sem levantar a TUI
// inteira -- Station/FTXUI/pty) para o mesmo padrao de EdlSyntaxHighlight.hpp:
// so' string/FTXUI, testavel isolado.
//------------------------------------------------------------------------------
namespace app {

// Numero de bytes do glifo UTF-8 que comeca em text[pos] -- mesma logica de
// EatCodePoint()/GlyphNext() do FTXUI (ftxui/screen/string.cpp), replicada
// aqui pra nao depender de include privado da lib. ACHADO POR AUDITORIA (nao
// redescobrir): sem isto, cortar 'o glifo sob o cursor' com
// 'text.substr(pos, 1)' (sempre 1 BYTE) faz qualquer caractere multibyte
// (acento -- convencao pt-BR deste projeto, texto livre no editor) sumir da
// tela quando o cursor cai sobre ele -- os dois pedacos (lead byte isolado +
// continuation byte isolado) sao UTF-8 invalido, e o pipeline de glifos do
// FTXUI descarta os dois em silencio. Glifo malformado (lead byte invalido,
// ou 'pos' caindo num continuation byte) degrada pra 1 byte -- nunca estoura
// o tamanho da string.
std::string::size_type utf8GlyphBytes(const std::string& text, std::string::size_type pos);

// Uma linha da previa colorida -- tokeniza SO' esta linha (comentario/
// pontuacao nunca atravessam '\n', e string multi-linha e' caso extremo
// nunca visto num '.edl' de producao deste repositorio; aceitavel nao
// colorir esse caso raro em troca de nao ter que rastrear deslocamento
// absoluto entre linhas). Quando 'isCursorLine', localiza o TOKEN que
// contem 'cursorCol' (ou o fim da linha) e corta so' ELE em ate 3 pedacos,
// preservando a cor nos tres -- mesma forma de
// 'ftxui::InputBase::OnRender()' (input.cpp), so' por token em vez de uma
// unica 'Text(linha)'. O pedaco do meio carrega 'cursorDecorator' -- o
// MESMO 'focus'/'focusCursorBarBlinking' que o Input nativo aplicaria --
// pra 'frame()' (por fora, no editorBox de edlTab) continuar sabendo pra
// onde rolar.
ftxui::Element renderEdlLine(const std::string& line, bool isCursorLine,
                             std::string::size_type cursorCol,
                             const ftxui::Decorator& cursorDecorator);

// Reconstroi a MESMA forma que 'ftxui::Input' monta por dentro (uma linha
// por '\n', o glifo do cursor marcado com o decorador de foco certo, 'vbox'
// + 'frame' por fora pra rolar -- ver input.cpp, InputBase::OnRender()), so'
// que colorindo por TOKEN em vez de uma unica 'text(linha)' por linha.
// Precisa da posicao do cursor (nao so' do texto) porque, sem marcar o
// glifo certo com 'focus'/'focusCursorBarBlinking', o 'frame()' de fora
// (editorBox, em edlTab) nao teria pra ONDE rolar num arquivo maior que a
// tela -- exatamente a mesma marcacao que o Input nativo ja fazia, so' que
// aqui refeita a mao porque DashboardLoop.cpp descarta o 'InputState::element'
// dele (ver o comentario grande em 'edlInputOpt.transform', DashboardLoop.cpp).
ftxui::Element renderHighlightedEdlText(const std::string& source, int cursorPosition,
                                        bool focused, bool hovered);

} // namespace app
