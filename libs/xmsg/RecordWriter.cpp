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
   put(value);      // os rotulos vem de nomes de player/lado/modo: sem aspas nem barras
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
