#include "app/EdlHighlightRender.hpp"

#include <algorithm>
#include <cstdint>

namespace app {

using namespace ftxui;

namespace {

// Cor + span por token -- detalhe de implementacao de renderEdlLine(), sem
// uso fora deste arquivo (ao contrario de utf8GlyphBytes()/renderEdlLine()/
// renderHighlightedEdlText(), a API publica declarada no header).
Color edlTokenColor(const EdlTokenKind kind)
{
   switch (kind) {
      case EdlTokenKind::Comment:   return Color::RGB(0x7E, 0x89, 0x83);
      case EdlTokenKind::String:    return Color::RGB(0x8F, 0xCB, 0x9E);
      case EdlTokenKind::ClassName: return Color::RGB(0x9D, 0xBB, 0xE3);
      case EdlTokenKind::SlotName:  return Color::RGB(0xE8, 0xC2, 0x84);
      case EdlTokenKind::Bool:      return Color::RGB(0xD9, 0x8A, 0x4A);
      case EdlTokenKind::Num:       return Color::RGB(0xC3, 0xAD, 0xEA);
      case EdlTokenKind::Punct:     return Color::RGB(0x8B, 0x96, 0x8E);
      case EdlTokenKind::Value:
      case EdlTokenKind::Plain:
      default:                    return Color::RGB(0xE7, 0xEA, 0xE4);
   }
}

Element edlTokenSpan(const std::string& piece, const EdlTokenKind kind)
{
   Element e{text(piece) | color(edlTokenColor(kind))};
   if (kind == EdlTokenKind::Comment) e = e | dim;
   if (kind == EdlTokenKind::ClassName) e = e | bold;
   return e;
}

} // namespace

std::string::size_type utf8GlyphBytes(const std::string& text, const std::string::size_type pos)
{
   if (pos >= text.size()) return 1;
   const auto c0{static_cast<std::uint8_t>(text[pos])};

   // Mesma tabela de bits que ftxui::EatCodePoint() (screen/string.cpp) usa
   // para achar onde um glifo UTF-8 termina -- replicada aqui porque o
   // header que a declara (screen/string_internal.hpp) mora em src/, nao em
   // include/: nao e' publico, o SDK deste projeto nao pode depender dele.
   // Qualquer sequencia incompleta/invalida (start byte de continuacao
   // isolado, ou faltam bytes ate o fim da string) degrada pra 1 byte --
   // igual ao 'return false' da funcao real.
   std::string::size_type width{1};
   if ((c0 & 0b1000'0000) == 0b0000'0000) {
      width = 1;
   } else if ((c0 & 0b1110'0000) == 0b1100'0000 && pos + 1 < text.size()) {
      width = 2;
   } else if ((c0 & 0b1111'0000) == 0b1110'0000 && pos + 2 < text.size()) {
      width = 3;
   } else if ((c0 & 0b1111'1000) == 0b1111'0000 && pos + 3 < text.size()) {
      width = 4;
   } else {
      width = 1;
   }
   return std::min(width, text.size() - pos);
}

Element renderEdlLine(const std::string& line, const bool isCursorLine,
                      const std::string::size_type cursorCol, const Decorator& cursorDecorator)
{
   Elements spans;
   const std::vector<EdlToken> tokens{tokenizeEdlText(line)};

   if (!isCursorLine) {
      for (const EdlToken& tok : tokens) spans.push_back(edlTokenSpan(tok.text, tok.kind));
      return spans.empty() ? Element{text("")} : hbox(std::move(spans));
   }

   std::string::size_type offset{0};
   bool placed{false};
   for (const EdlToken& tok : tokens) {
      const std::string::size_type len{tok.text.size()};
      if (!placed && cursorCol >= offset && cursorCol < offset + len) {
         const std::string::size_type local{cursorCol - offset};
         // ACHADO POR AUDITORIA (nao redescobrir): esta era a linha
         // 'tok.text.substr(local, 1)' -- sempre 1 BYTE. Um caractere
         // acentuado (2 bytes em UTF-8) sob o cursor virava um lead byte
         // isolado ('before') e um continuation byte isolado ('atCursor'),
         // os dois UTF-8 invalido -- ftxui::Screen descarta os dois em
         // silencio no proximo redesenho, e o glifo simplesmente some da
         // tela enquanto o cursor estiver sobre ele. 'utf8GlyphBytes()'
         // acha a largura certa (ver o comentario dela).
         const std::string::size_type glyphLen{utf8GlyphBytes(tok.text, local)};
         const std::string before{tok.text.substr(0, local)};
         const std::string atCursor{tok.text.substr(local, glyphLen)};
         const std::string after{tok.text.substr(local + glyphLen)};
         if (!before.empty()) spans.push_back(edlTokenSpan(before, tok.kind));
         spans.push_back(edlTokenSpan(atCursor, tok.kind) | cursorDecorator);
         if (!after.empty()) spans.push_back(edlTokenSpan(after, tok.kind));
         placed = true;
      } else {
         spans.push_back(edlTokenSpan(tok.text, tok.kind));
      }
      offset += len;
   }
   if (!placed) spans.push_back(text(" ") | cursorDecorator); // cursor no fim da linha (ou linha vazia)
   return hbox(std::move(spans)) | xflex;
}

Element renderHighlightedEdlText(const std::string& source, const int cursorPosition,
                                 const bool focused, const bool hovered)
{
   const auto cursorPos{static_cast<std::string::size_type>(
      std::clamp(cursorPosition, 0, static_cast<int>(source.size())))};

   std::vector<std::string> lines;
   {
      std::string::size_type start{0};
      while (true) {
         const std::string::size_type nl{source.find('\n', start)};
         lines.push_back(source.substr(start, nl == std::string::npos ? std::string::npos : nl - start));
         if (nl == std::string::npos) break;
         start = nl + 1;
      }
   }

   int cursorLine{0};
   std::string::size_type cursorCharIndex{cursorPos};
   for (const auto& line : lines) {
      if (cursorCharIndex <= line.size()) break;
      cursorCharIndex -= line.size() + 1;
      ++cursorLine;
   }

   // Mesma escolha de decorador que InputBase::OnRender() faz (ver
   // input.cpp) -- 'insert()' e' sempre 'true' aqui (nunca alternado por
   // nenhuma tecla desta aba), entao o ramo 'focusCursorBlockBlinking' (so'
   // usado no modo overtype) nunca se aplica.
   const Decorator cursorDecorator{(!focused && !hovered) ? focus : focusCursorBarBlinking};

   Elements rendered;
   rendered.reserve(lines.size());
   for (std::size_t i{0}; i < lines.size(); ++i) {
      rendered.push_back(
         renderEdlLine(lines[i], static_cast<int>(i) == cursorLine, cursorCharIndex, cursorDecorator));
   }
   return vbox(std::move(rendered)) | frame;
}

} // namespace app
