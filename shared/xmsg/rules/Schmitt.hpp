#pragma once

namespace mixr {
namespace xmsg {
namespace rules {

//------------------------------------------------------------------------------
// Schmitt -- limiar com histerese e tempo minimo de permanencia.
//
// POR QUE ESCREVER ISTO. O MIXR nao tem. Grep por
// hysteresis|Schmitt|Threshold|Debounce em include/mixr/ inteiro: zero. O que
// existe e base::deadband() (clamp sem memoria), linearsystem/ (suavizacao,
// nao decisao) e recorder::PrintSelected (1 campo x EQ/LT/GT, sem estado e
// print-only). Nenhum deles resolve o problema real de um canal de eventos.
//
// O PROBLEMA REAL. Um limiar simples ("avise quando a altitude AGL cair abaixo
// de 500 m") produz uma rajada de mensagens enquanto o valor oscila em cima da
// fronteira -- e valores de simulacao SEMPRE oscilam em cima da fronteira. Sao
// duas defesas, e elas resolvem coisas diferentes:
//
//   clear (histerese)  a condicao so DESARMA num valor folgado do de armar.
//                      Mata a oscilacao em torno da fronteira.
//   hold  (debounce)   a condicao so ARMA depois de t segundos continuos do
//                      lado ligado. Mata o transiente curto -- aqui isso
//                      importa de verdade, porque JSBSimModel::reset() nao
//                      roda FGTrim e a aeronave comeca destrimada, com um
//                      transiente energetico nos primeiros segundos.
//
// O tempo e SIMULADO (o dt que o chamador passa), nunca relogio de parede: e
// o que mantem a saida identica com 1, 2 e 4 threads de tempo critico.
//
// update() devolve true APENAS NA BORDA DE SUBIDA -- no instante em que a
// condicao passa a valer. Enquanto ela continua valendo, devolve false. Um
// canal de eventos que repetisse a mensagem a cada ciclo do plato seria um
// canal inutil.
//------------------------------------------------------------------------------
class Schmitt
{
public:
   enum class Sense { Above, Below };

   Schmitt() = default;

   // 'clear' e o valor de DESARME e tem de ficar do lado folgado de 'trip':
   // com Above, clear < trip; com Below, clear > trip. configure() devolve
   // false se isso nao valer -- quem chama recusa o slot (sem histerese o
   // evento repete na fronteira, que e exatamente o que esta classe existe
   // para impedir).
   bool configure(Sense sense, double trip, double clear, double holdSeconds);

   void reset();

   // 'valid' false = campo indisponivel. Nesse caso NAO avalia, NAO produz
   // borda e CONGELA o nivel logico anterior. Sem essa regra, um player sem
   // dynamicsModel (o bandit1 que chega por DIS e clonado de um template sem
   // ele) leria 0.0 em toda grandeza de motor e acusaria pane permanente.
   bool update(double dt, double value, bool valid);

   bool active() const           { return active_; }
   double holdRemaining() const  { return candidate_ ? (hold_ - holdTimer_) : 0.0; }

private:
   bool raw(double value) const;

   Sense sense_{Sense::Above};
   double trip_{};
   double clear_{};
   double hold_{};

   bool active_{};        // nivel logico: a condicao esta valendo
   bool candidate_{};     // cruzou, mas ainda cumprindo 'hold'
   double holdTimer_{};
};

} // namespace rules
} // namespace xmsg
} // namespace mixr
