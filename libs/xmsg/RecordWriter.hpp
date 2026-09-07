#pragma once

#include "xmsg/FieldCatalog.hpp"

#include <cstddef>

namespace mixr {
namespace xmsg {

//------------------------------------------------------------------------------
// RecordWriter -- monta UMA linha NDJSON num buffer fixo, reaproveitado.
//
// Nao ha std::ostringstream nem std::string por amostra de proposito: o
// subsistema inteiro nao aloca em regime, e este e o unico lugar do caminho
// quente que teria motivo para alocar.
//
// DUAS REGRAS DE HONESTIDADE, as duas sobre nao mentir com um numero:
//
//   1. Campo cujo grupo nao vale sai como 'null', nunca como 0.0. Zero e um
//      valor plausivel de empuxo; 'null' diz "esta aeronave nao tem motor
//      modelado" -- que e o caso do player recebido por DIS.
//   2. NaN e infinito viram 'null' tambem. JSON nao os representa, e escrever
//      'nan' produziria um arquivo que nenhum leitor aceita -- falha no
//      consumidor, longe da causa.
//
// Estouro de buffer trunca e marca overflow(); quem chama reporta isso na
// mensagem de saude em vez de emitir uma linha JSON cortada no meio.
//------------------------------------------------------------------------------
class RecordWriter
{
public:
   static constexpr std::size_t CAPACITY{8192};

   void begin(double t, const char* msgName);
   void addLabel(const char* key, const char* value);
   void addField(const FieldInfo& info, double value, bool valid);
   void addNumber(const char* key, double value);
   void addInt(const char* key, long value);
   void end();

   const char* data() const   { return buf_; }
   std::size_t size() const   { return len_; }
   bool overflow() const      { return overflow_; }

private:
   void put(const char* s);
   void putKey(const char* key);

   char buf_[CAPACITY]{};
   std::size_t len_{};
   bool overflow_{};
   bool first_{true};
};

} // namespace xmsg
} // namespace mixr
