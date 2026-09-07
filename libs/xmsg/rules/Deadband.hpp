#pragma once

namespace mixr {
namespace xmsg {
namespace rules {

//------------------------------------------------------------------------------
// Deadband -- "mudou o suficiente desde a ULTIMA VEZ QUE EU AVISEI".
//
// E a leitura correta de "me avise quando a altitude mudar", e a palavra que
// faz toda a diferenca e EMITIDA. Comparar com a amostra anterior daria uma
// mensagem por ciclo numa subida constante (cada passo muda um pouco);
// comparar com a ultima emitida coalesce sozinho: uma subida lenta produz uma
// mensagem por degrau de 'by', e uma subida rapida produz uma por degrau
// tambem. A taxa de saida passa a depender da GRANDEZA, nao da taxa de
// amostragem -- que e o que alguem quer dizer com "mudanca de altitude".
//
// Nao ha tempo aqui de proposito: quem limita a taxa e o EmitGate, e quem
// mede derivada e o RateWindow. Uma questao por classe.
//------------------------------------------------------------------------------
class Deadband
{
public:
   Deadband() = default;

   // 'by' <= 0 significa "qualquer mudanca" -- util para campos discretos
   // (modo do player, indice do motor pior, flag de crash), onde qualquer
   // diferenca ja e o evento.
   void configure(double by);

   void reset();

   // Devolve true quando esta amostra deve ser emitida; nesse caso ela passa
   // a ser a referencia das proximas comparacoes.
   //
   // A PRIMEIRA amostra valida sempre emite: sem uma referencia inicial nao
   // ha o que comparar, e comecar em zero mentiria (uma aeronave a 1750 m
   // reportaria "mudanca de 1750 m" no primeiro ciclo).
   bool update(double value, bool valid);

   bool hasReference() const  { return has_; }
   double reference() const   { return lastEmitted_; }

private:
   double by_{};
   bool has_{};
   double lastEmitted_{};
};

} // namespace rules
} // namespace xmsg
} // namespace mixr
