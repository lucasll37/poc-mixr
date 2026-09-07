#pragma once

namespace mixr {
namespace xmsg {
namespace rules {

//------------------------------------------------------------------------------
// EmitGate -- o intervalo minimo entre emissoes ('every:' do EDL).
//
// A DECISAO QUE ESTA CLASSE CARREGA: um teto de taxa pode DESCARTAR o que nao
// coube, ou ADIAR. Aqui ele adia.
//
// Descartar e o default errado para um canal de eventos. A borda de "o aviao
// bateu" ou "o motor caiu" chega uma vez; se ela cair dentro do intervalo
// minimo de uma mensagem configurada com 'every: ( Seconds 5 )', descartar
// significa perder exatamente o evento que justifica o canal existir -- e
// perder em silencio, o que e pior. Adiar custa latencia (ate 'every'
// segundos) e nao perde nada.
//
// Para mensagem PERIODICA (sem 'when:'), adiar e descartar dao no mesmo: a
// proxima amostra vem logo e e igualmente boa. E por isso que uma classe so
// serve para os dois casos.
//
// deferred() conta quantas vezes algo quis sair e nao saiu. Esse numero vai
// para a mensagem interna de saude: um canal que silencia por saturacao tem
// de DIZER que silenciou, senao mente por omissao.
//
// O relogio e de tempo SIMULADO. E deliberado: mantem a semantica igual em
// tempo real, em passo fixo e sob fastForwardRate, e mantem a saida
// deterministica.
//------------------------------------------------------------------------------
class EmitGate
{
public:
   EmitGate() = default;

   // <= 0 desliga o piso (emite sempre que houver o que emitir).
   void configure(double minIntervalSeconds);

   void reset();

   // 'wants' = ha uma borda ou uma amostra periodica querendo sair agora.
   // Devolve true se a emissao deve acontecer NESTE passo.
   bool update(double dt, bool wants);

   long deferred() const   { return deferred_; }
   bool pending() const    { return pending_; }

private:
   double minInterval_{};
   double sinceLast_{};
   bool pending_{};
   long deferred_{};
};

} // namespace rules
} // namespace xmsg
} // namespace mixr
