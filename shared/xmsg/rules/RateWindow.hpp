#pragma once

#include <cstddef>

namespace mixr {
namespace xmsg {
namespace rules {

//------------------------------------------------------------------------------
// RateWindow -- derivada de um campo sobre uma janela de tempo SIMULADO.
//
// Existe para expressar condicoes que um limiar sobre o valor nao alcanca:
// "descendo mais rapido que 10 m/s" nao e "abaixo de X metros". Duas
// aeronaves na mesma altitude, uma estabilizada e outra em queda, sao
// situacoes diferentes, e so a derivada as separa.
//
// POR QUE JANELA, E NAO DOIS PONTOS. A diferenca entre amostras consecutivas
// dividida por dt e ruidosa: com dt de 20 ms, uma oscilacao de decimetro vira
// varios m/s. A janela suaviza sem filtro nenhum -- e so a inclinacao da
// corda entre a amostra mais velha ainda dentro da janela e a mais nova.
//
// O buffer e FIXO (sem alocacao em regime, requisito do subsistema inteiro).
// Se a janela pedir mais amostras do que cabem, as mais velhas caem e a
// janela efetiva encurta -- ready() continua true e a derivada continua
// correta para o intervalo que sobrou; e degradacao honesta, nao erro.
//------------------------------------------------------------------------------
class RateWindow
{
public:
   static constexpr std::size_t CAPACITY{256};

   RateWindow() = default;

   void configure(double windowSeconds);
   void reset();

   // Acumula o tempo simulado e guarda a amostra. Amostra invalida nao entra
   // (a janela congela, como no Schmitt e no Deadband).
   void push(double dt, double value, bool valid);

   // Ha dois pontos separados por tempo > 0 DENTRO DA JANELA?
   bool ready() const;

   // Unidades do campo por segundo. Zero se !ready().
   double rate() const;

private:
   struct Sample { double t{}; double v{}; };

   // Indice da amostra mais VELHA ainda dentro de 'window_', partindo da mais
   // nova. Compartilhado por ready()/rate() de proposito -- os dois tem de
   // concordar sobre o que "dentro da janela" significa; ready() checando so
   // "existe alguma separacao no buffer inteiro" (sem bater contra window_)
   // já mentiu no passado: com window_ menor que o intervalo entre amostras
   // (janela mal configurada, ou uma pausa/retomada), ready() dizia que havia
   // derivada e rate() devolvia 0.0 por falta de separacao DENTRO da janela --
   // exatamente o "derivada zero" que MsgRate::evaluate() existe para evitar.
   std::size_t oldestInWindow(std::size_t novo) const;

   double window_{1.0};
   double clock_{};

   Sample buf_[CAPACITY]{};
   std::size_t head_{};    // proxima posicao de escrita
   std::size_t count_{};
};

} // namespace rules
} // namespace xmsg
} // namespace mixr
