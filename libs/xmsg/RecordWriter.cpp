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
   // ACHADO POR AUDITORIA (nao redescobrir): 'value' pode vir do NOME de um
   // player recebido por DIS -- EntityMarking::marking, 11 bytes CRUS
   // controlados por quem envia o PDU na rede (ver
   // contexts/src/mixr/src/interop/dis/NetIO_entity_state.cpp), copiado sem
   // filtragem ate aqui (SnapshotSource::copyName() so trunca por tamanho).
   // Sem escapar, um nome malicioso tipo 'x","q":1' injeta um campo JSON
   // inteiro na linha gravada -- confirmado reproduzindo com json.loads().
   // Escapa char a char, sem alocar (mesma filosofia de buffer fixo desta
   // classe -- ver o cabecalho do .hpp).
   for (const char* p{value}; *p != '\0'; ++p) {
      const unsigned char c{static_cast<unsigned char>(*p)};
      switch (c) {
         case '"':  put("\\\""); break;
         case '\\': put("\\\\"); break;
         case '\n': put("\\n"); break;
         case '\r': put("\\r"); break;
         case '\t': put("\\t"); break;
         default:
            if (c < 0x20) {
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
