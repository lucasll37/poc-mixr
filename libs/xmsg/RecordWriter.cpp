#include "xmsg/RecordWriter.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace mixr {
namespace xmsg {

void RecordWriter::put(const char* const s)
{
   if (s == nullptr) return;
   const std::size_t n{std::strlen(s)};
   if (len_ + n + 1 >= CAPACITY) { overflow_ = true; return; }
   std::memcpy(buf_ + len_, s, n);
   len_ += n;
   buf_[len_] = '\0';
}

namespace {

// O escape de addLabel() cobre aspas, barra e controles ASCII (<0x20), mas
// 'value' pode conter bytes >= 0x80 sem garantia de charset (ex.:
// EntityMarking::marking, bytes crus de um PDU DIS) -- sem validacao, isso
// produz uma linha .jsonl que nao e UTF-8 valido. isValidUtf8() faz
// validacao minima de forma (lead byte + bytes de continuacao 10xxxxxx)
// para decidir se e' seguro deixar os bytes crus, preservando texto pt-BR
// legitimo (acentos sao UTF-8 multi-byte valido).
bool isValidUtf8(const char* const s)
{
   auto* p{reinterpret_cast<const unsigned char*>(s)};
   while (*p != '\0') {
      int extra{};
      if ((*p & 0x80) == 0x00)      extra = 0;   // ASCII
      else if ((*p & 0xE0) == 0xC0) extra = 1;
      else if ((*p & 0xF0) == 0xE0) extra = 2;
      else if ((*p & 0xF8) == 0xF0) extra = 3;
      else return false;                          // lead byte invalido

      ++p;
      for (int i{}; i < extra; ++i, ++p) {
         if ((*p & 0xC0) != 0x80) return false;    // byte de continuacao ausente/invalido
      }
   }
   return true;
}

} // namespace

void RecordWriter::putChar(const char c)
{
   if (len_ + 1 >= CAPACITY) { overflow_ = true; return; }
   buf_[len_++] = c;
   buf_[len_] = '\0';
}

void RecordWriter::putKey(const char* const key)
{
   if (!first_) put(",");
   first_ = false;
   put("\"");
   put(key);
   put("\":");
}

void RecordWriter::begin(const double t, const char* const msgName)
{
   len_ = 0;
   buf_[0] = '\0';
   overflow_ = false;
   first_ = true;

   put("{");
   // 't' e SEMPRE tempo simulado. Nunca relogio de parede, nunca id de thread:
   // e o que faz a saida ser identica com 1, 2 e 4 threads de tempo critico.
   addNumber("t", t);
   putKey("msg");
   put("\"");
   put(msgName);
   put("\"");
}

void RecordWriter::addLabel(const char* const key, const char* const value)
{
   if (value == nullptr || value[0] == '\0') return;
   putKey(key);
   put("\"");
   // 'value' pode vir do nome de um player recebido por DIS
   // (EntityMarking::marking, controlado por quem envia o PDU na rede),
   // copiado sem filtragem ate aqui -- sem escapar, um nome malicioso do
   // tipo 'x","q":1' injeta um campo JSON inteiro na linha gravada. Escapa
   // caractere a caractere, sem alocar.
   const bool utf8Ok{isValidUtf8(value)};
   for (const char* p{value}; *p != '\0'; ++p) {
      const unsigned char c{static_cast<unsigned char>(*p)};
      switch (c) {
         case '"':  put("\\\""); break;
         case '\\': put("\\\\"); break;
         case '\n': put("\\n"); break;
         case '\r': put("\\r"); break;
         case '\t': put("\\t"); break;
         default:
            if (c < 0x20 || (!utf8Ok && c >= 0x80)) {
               char esc[8]{};
               std::snprintf(esc, sizeof(esc), "\\u%04x", c);
               put(esc);
            } else {
               putChar(static_cast<char>(c));
            }
      }
   }
   put("\"");
}

void RecordWriter::addNumber(const char* const key, const double value)
{
   putKey(key);
   if (!std::isfinite(value)) { put("null"); return; }

   char tmp[40]{};
   std::snprintf(tmp, sizeof(tmp), "%.9g", value);
   put(tmp);
}

void RecordWriter::addInt(const char* const key, const long value)
{
   putKey(key);
   char tmp[32]{};
   std::snprintf(tmp, sizeof(tmp), "%ld", value);
   put(tmp);
}

void RecordWriter::addField(const FieldInfo& info, const double value, const bool valid)
{
   if (!valid) { putKey(info.name); put("null"); return; }
   addNumber(info.name, value);
}

void RecordWriter::end()
{
   put("}");
}

} // namespace xmsg
} // namespace mixr
