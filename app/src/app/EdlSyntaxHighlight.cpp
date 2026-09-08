#include "app/EdlSyntaxHighlight.hpp"

#include <cctype>
#include <optional>

namespace app {

namespace {

bool isIdentChar(const char c)
{
   // EDL_IDENT_CHARS de src/ui/edl_builder_core.js:
   // "-a-zA-Z0-9~!@#$%^&*_+=<>?/"
   if (std::isalnum(static_cast<unsigned char>(c)) != 0) return true;
   switch (c) {
      case '-': case '~': case '!': case '@': case '#': case '$': case '%':
      case '^': case '&': case '*': case '_': case '+': case '=': case '<':
      case '>': case '?': case '/':
         return true;
      default:
         return false;
   }
}

bool isFormWs(const char c)
{
   return c == ' ' || c == ',' || c == '\t' || c == '\v' || c == '\f';
}

bool isNumSuffix(const char c)
{
   return c == 'u' || c == 'U' || c == 'l' || c == 'L';
}

bool isDigit(const char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; }

//------------------------------------------------------------------------------
// Cada 'tryX' devolve o indice (EXCLUSIVO) onde o casamento termina, ou
// std::string::npos se a alternativa nao casa comecando em 'i' -- a mesma
// semantica "tenta, falha rapido" da alternancia ORDENADA do regex
// original: a primeira alternativa que casar em 'i', no LOOP principal mais
// abaixo, vence -- mesmo que uma alternativa POSTERIOR casasse um trecho
// mais longo (regex nao faz "longest match" entre alternativas de um
// grupo, so' ordem).
//------------------------------------------------------------------------------

std::string::size_type tryComment(const std::string& text, const std::string::size_type i)
{
   if (i + 1 >= text.size() || text[i] != '/' || text[i + 1] != '/') return std::string::npos;
   std::string::size_type j{i};
   while (j < text.size() && text[j] != '\n') ++j;
   return j;
}

std::string::size_type tryDqString(const std::string& text, const std::string::size_type i)
{
   if (text[i] != '"') return std::string::npos;
   std::string::size_type j{i + 1};
   while (j < text.size()) {
      if (text[j] == '\\') {
         if (j + 1 >= text.size()) return std::string::npos;
         j += 2;
         continue;
      }
      if (text[j] == '"') return j + 1;
      ++j;
   }
   return std::string::npos; // sem aspa de fechamento -- alternativa falha
}

std::string::size_type tryLtString(const std::string& text, const std::string::size_type i)
{
   if (text[i] != '<') return std::string::npos;
   // Lookahead do regex original ("(?=[^<>\n]*>)"): precisa achar um '>'
   // antes de outro '<' ou de uma quebra de linha, senao a alternativa
   // inteira falha (o '<' fica de fora de qualquer token).
   bool found{false};
   for (std::string::size_type k{i + 1}; k < text.size(); ++k) {
      const char c{text[k]};
      if (c == '>') { found = true; break; }
      if (c == '<' || c == '\n') break;
   }
   if (!found) return std::string::npos;
   std::string::size_type j{i + 1};
   while (j < text.size()) {
      if (text[j] == '\\') {
         if (j + 1 >= text.size()) return std::string::npos;
         j += 2;
         continue;
      }
      if (text[j] == '>') return j + 1;
      ++j;
   }
   return std::string::npos;
}

struct FormOpenMatch {
   std::string::size_type wsStart{};
   std::string::size_type wsEnd{};
   std::string::size_type classStart{};
   std::string::size_type classEnd{};
};

// '(' + espaco/virgula opcional + nome de classe (>= 1 char de identificador).
std::optional<FormOpenMatch> tryFormOpenClass(const std::string& text, const std::string::size_type i)
{
   if (text[i] != '(') return std::nullopt;
   std::string::size_type j{i + 1};
   const std::string::size_type wsStart{j};
   while (j < text.size() && isFormWs(text[j])) ++j;
   const std::string::size_type classStart{j};
   while (j < text.size() && isIdentChar(text[j])) ++j;
   if (j == classStart) return std::nullopt; // classe exige >= 1 caractere
   return FormOpenMatch{wsStart, classStart, classStart, j};
}

// identificador + ':' -- devolve o indice do ':'. Tentada ANTES de 'bool' e
// dos numeros (ver a ordem no loop principal), entao um nome de slot que
// por acaso se pareca com um literal (ex.: "true:", "42:") continua slot.
std::optional<std::string::size_type> trySlotName(const std::string& text, const std::string::size_type i)
{
   if (!isIdentChar(text[i])) return std::nullopt;
   std::string::size_type j{i};
   while (j < text.size() && isIdentChar(text[j])) ++j;
   if (j >= text.size() || text[j] != ':') return std::nullopt;
   return j;
}

std::string::size_type tryBool(const std::string& text, const std::string::size_type i)
{
   static const char* const kCandidates[]{"true", "TRUE", "false", "FALSE"};
   for (const char* const cand : kCandidates) {
      const std::string::size_type len{std::string::traits_type::length(cand)};
      if (text.compare(i, len, cand) != 0) continue;
      const std::string::size_type after{i + len};
      if (after < text.size() && isIdentChar(text[after])) continue; // lookahead falhou
      return after;
   }
   return std::string::npos;
}

std::string::size_type tryHexNum(const std::string& text, const std::string::size_type i)
{
   if (i + 1 >= text.size() || text[i] != '0' || (text[i + 1] != 'x' && text[i + 1] != 'X')) {
      return std::string::npos;
   }
   std::string::size_type j{i + 2};
   const std::string::size_type hexStart{j};
   while (j < text.size() && std::isxdigit(static_cast<unsigned char>(text[j])) != 0) ++j;
   if (j == hexStart) return std::string::npos;
   while (j < text.size() && isNumSuffix(text[j])) ++j;
   if (j < text.size() && isIdentChar(text[j])) return std::string::npos;
   return j;
}

// As tres formas de EDL_TOKEN_RE, na mesma ordem interna:
//   \d*\.\d+(?:[Ee][+-]?\d+)?   (".5", "12.5", "12.5e10")
//   \d+\.\d*(?:[Ee][+-]?\d+)?   ("5.", "5.e10")
//   \d+[Ee][+-]?\d+             ("5e10", sem ponto)
// com sinal opcional na frente e sufixo [fFlL] opcional no final.
std::string::size_type tryFloatNum(const std::string& text, const std::string::size_type i)
{
   std::string::size_type j{i};
   if (j < text.size() && (text[j] == '+' || text[j] == '-')) ++j;

   std::string::size_type k{j};
   while (k < text.size() && isDigit(text[k])) ++k;
   const bool hasIntDigits{k > j};

   std::string::size_type end{std::string::npos};
   if (k < text.size() && text[k] == '.') {
      const std::string::size_type fracStart{k + 1};
      std::string::size_type f{fracStart};
      while (f < text.size() && isDigit(text[f])) ++f;
      const bool hasFracDigits{f > fracStart};
      if (hasFracDigits || hasIntDigits) end = f; // \d*\.\d+ OU \d+\.\d*
   }

   if (end == std::string::npos && hasIntDigits && k < text.size() && (text[k] == 'e' || text[k] == 'E')) {
      // \d+[Ee][+-]?\d+ -- sem ponto decimal, expoente OBRIGATORIO aqui.
      std::string::size_type e{k + 1};
      if (e < text.size() && (text[e] == '+' || text[e] == '-')) ++e;
      const std::string::size_type expStart{e};
      while (e < text.size() && isDigit(text[e])) ++e;
      if (e > expStart) end = e;
   }
   if (end == std::string::npos) return std::string::npos;

   // Expoente opcional (so' se casado pela forma com ponto -- a forma sem
   // ponto ja' exigiu o dela acima; tentar de novo aqui e' inofensivo,
   // so' nao encontra nada).
   if (end < text.size() && (text[end] == 'e' || text[end] == 'E')) {
      std::string::size_type e{end + 1};
      if (e < text.size() && (text[e] == '+' || text[e] == '-')) ++e;
      const std::string::size_type expStart{e};
      while (e < text.size() && isDigit(text[e])) ++e;
      if (e > expStart) end = e;
   }

   if (end < text.size() && (text[end] == 'f' || text[end] == 'F' || text[end] == 'l' || text[end] == 'L')) ++end;
   if (end < text.size() && isIdentChar(text[end])) return std::string::npos;
   return end;
}

std::string::size_type tryOctalNum(const std::string& text, const std::string::size_type i)
{
   if (text[i] != '0') return std::string::npos;
   std::string::size_type j{i + 1};
   const std::string::size_type start{j};
   while (j < text.size() && isDigit(text[j])) ++j;
   if (j == start) return std::string::npos; // "0" sozinho nao e' octal aqui, e' intnum
   while (j < text.size() && isNumSuffix(text[j])) ++j;
   if (j < text.size() && isIdentChar(text[j])) return std::string::npos;
   return j;
}

std::string::size_type tryIntNum(const std::string& text, const std::string::size_type i)
{
   std::string::size_type j{i};
   if (j < text.size() && (text[j] == '+' || text[j] == '-')) ++j;
   const std::string::size_type start{j};
   while (j < text.size() && isDigit(text[j])) ++j;
   if (j == start) return std::string::npos;
   while (j < text.size() && isNumSuffix(text[j])) ++j;
   if (j < text.size() && isIdentChar(text[j])) return std::string::npos;
   return j;
}

std::string::size_type tryIdent(const std::string& text, const std::string::size_type i)
{
   if (!isIdentChar(text[i])) return std::string::npos;
   std::string::size_type j{i};
   while (j < text.size() && isIdentChar(text[j])) ++j;
   return j;
}

} // namespace

std::vector<EdlToken> tokenizeEdlText(const std::string& text)
{
   std::vector<EdlToken> tokens;
   const std::string::size_type n{text.size()};
   std::string::size_type i{0};
   std::string::size_type plainStart{0};

   const auto flushPlain{[&](const std::string::size_type upTo) {
      if (upTo > plainStart) tokens.push_back({text.substr(plainStart, upTo - plainStart), EdlTokenKind::Plain});
   }};
   const auto emit{[&](const std::string::size_type start, const std::string::size_type end, const EdlTokenKind kind) {
      flushPlain(start);
      tokens.push_back({text.substr(start, end - start), kind});
      i = end;
      plainStart = i;
   }};

   while (i < n) {
      if (const std::string::size_type end{tryComment(text, i)}; end != std::string::npos) {
         emit(i, end, EdlTokenKind::Comment);
         continue;
      }
      if (const std::string::size_type end{tryDqString(text, i)}; end != std::string::npos) {
         emit(i, end, EdlTokenKind::String);
         continue;
      }
      if (const std::string::size_type end{tryLtString(text, i)}; end != std::string::npos) {
         emit(i, end, EdlTokenKind::String);
         continue;
      }
      if (const auto m{tryFormOpenClass(text, i)}) {
         flushPlain(i);
         tokens.push_back({"(", EdlTokenKind::Punct});
         if (m->wsEnd > m->wsStart) {
            tokens.push_back({text.substr(m->wsStart, m->wsEnd - m->wsStart), EdlTokenKind::Plain});
         }
         tokens.push_back({text.substr(m->classStart, m->classEnd - m->classStart), EdlTokenKind::ClassName});
         i = m->classEnd;
         plainStart = i;
         continue;
      }
      if (const auto colonPos{trySlotName(text, i)}) {
         flushPlain(i);
         tokens.push_back({text.substr(i, *colonPos - i), EdlTokenKind::SlotName});
         tokens.push_back({":", EdlTokenKind::Punct});
         i = *colonPos + 1;
         plainStart = i;
         continue;
      }
      if (const std::string::size_type end{tryBool(text, i)}; end != std::string::npos) {
         emit(i, end, EdlTokenKind::Bool);
         continue;
      }
      if (const std::string::size_type end{tryHexNum(text, i)}; end != std::string::npos) {
         emit(i, end, EdlTokenKind::Num);
         continue;
      }
      if (const std::string::size_type end{tryFloatNum(text, i)}; end != std::string::npos) {
         emit(i, end, EdlTokenKind::Num);
         continue;
      }
      if (const std::string::size_type end{tryOctalNum(text, i)}; end != std::string::npos) {
         emit(i, end, EdlTokenKind::Num);
         continue;
      }
      if (const std::string::size_type end{tryIntNum(text, i)}; end != std::string::npos) {
         emit(i, end, EdlTokenKind::Num);
         continue;
      }
      if (text[i] == ')') { emit(i, i + 1, EdlTokenKind::Punct); continue; }
      if (text[i] == '{' || text[i] == '}') { emit(i, i + 1, EdlTokenKind::Punct); continue; }
      if (text[i] == '[' || text[i] == ']') { emit(i, i + 1, EdlTokenKind::Punct); continue; }
      if (const std::string::size_type end{tryIdent(text, i)}; end != std::string::npos) {
         emit(i, end, EdlTokenKind::Value);
         continue;
      }
      ++i; // nenhuma alternativa casa aqui -- o caractere fica no proximo trecho "plain"
   }
   flushPlain(n);
   return tokens;
}

} // namespace app
