#pragma once

#include "xrandom/DeterministicRng.hpp"

#include <cstdint>

namespace domain {

//------------------------------------------------------------------------------
// AerobaticPlan -- QUANDO fazer uma acrobacia, e por quanto tempo mante-la.
//
// Regra de negocio pura (sem MIXR, sem BehaviorTree.CPP, testavel isolada),
// no MESMO molde de domain::PatrolPlan: a classe so semeia e sorteia; nao
// sabe de semente mestra, nome de player nem salt de proposito. A semente
// que chega por setSeed() ja e' o resultado FINAL da hierarquia de derivacao
// (semente do cenario -> hash do nome do player -> salt de proposito)
// calculada por quem chama -- ver BtBehavior::configurePlans() e
// libs/xrandom/DeterministicRng.hpp.
//
// O GERADOR e' o mixr::xrandom::Rng, nao um std::mt19937_64 proprio: toda a
// aleatoriedade deste repositorio passa por libs/xrandom, e nenhuma outra
// classe instancia um gerador. Este header e' o UNICO do SDK que domain/
// inclui, e pode porque e' header-only e sem dependencia nenhuma -- os alvos
// test_domain/test_tree recebem so o CAMINHO DE INCLUDE do SDK
// (partial_dependency), nunca o link.
//
// DETERMINISMO -- a regra que o desenho obedece. O sorteio acontece num
// EVENTO DISCRETO (o fim de uma manobra), NUNCA por 'dt'. E' o mesmo
// invariante que PatrolPlan documenta para o jitter de rumo: o numero de
// manobras que um player executa nao depende de quantas threads de tempo
// critico existem, entao consumir o RNG so nessa borda mantem a sequencia
// identica com 1, 2 e 4 threads. A contagem regressiva ate a proxima manobra
// usa 'dt', mas 'dt' por frame e' o passo do FRAME, nao do agendador -- ele
// e' o mesmo nas tres configuracoes.
//
// O ANGULO ACUMULADO, e por que nao se compara o banco absoluto contra 360:
// o banco lido do JSBSim (attitude/phi-rad, via Player::getRollD()) e' um
// atan2 SEM wrap, ou seja vive em (-180, 180]. Comparar esse valor contra
// 360 nunca dispararia. update() integra a DIFERENCA entre ticks, passada
// por wrap180() -- a 50 Hz, mesmo a 200 deg/s sao 4 graus por tick, muito
// longe dos 180 que quebrariam a integracao.
//
// A GUARDA DE TEMPO nao e' luxo. Se a aeronave nao tiver autoridade de
// rolagem suficiente para fechar o giro (o caso real: o nivelador de asas de
// a4ap.xml zera o aileron liquido a partir de ~50-72 graus de banco quando
// nao esta gateado), sem timeout a manobra NUNCA termina e o player fica
// preso em rolagem para sempre, sem erro nenhum. Com timeout, o mesmo
// defeito vira um sintoma observavel: a manobra aborta e o rotulo volta.
//------------------------------------------------------------------------------
class AerobaticPlan
{
public:
   enum class Phase { Idle, Rolling };

   AerobaticPlan() = default;

   // stickCommand em [-1, 1]: sinal define o sentido do giro (direita > 0),
   // magnitude define quanto aileron. ZERO DESLIGA o recurso -- e' o default
   // do slot, entao um cenario que nao declara nada continua byte a byte
   // identico ao de hoje.
   //
   // minIntervalSec >= maxIntervalSec vira intervalo FIXO (sem sorteio), o
   // que mantem o recurso utilizavel de forma totalmente deterministica.
   void configure(double minIntervalSec, double maxIntervalSec,
                  double stickCommand, double timeoutSec);

   // 'seed' e' o resultado final ja derivado -- ver o comentario de classe.
   void setSeed(std::uint64_t seed);

   void reset();

   // Avanca a maquina de estados. 'rollDeg' e' o banco atual em (-180, 180].
   // Devolve true enquanto a manobra esta EM CURSO.
   bool update(double dt, double rollDeg);

   bool rolling() const              { return phase_ == Phase::Rolling; }

   // Comando de aileron a aplicar NESTE tick: stickCommand_ durante a
   // manobra, 0 fora dela (o comando e' pegajoso do lado do JSBSim -- quem
   // atua precisa de um valor explicito para zerar).
   double stick() const;

   double accumulatedRollDeg() const { return accumulatedDeg_; }
   double timeToNextSec() const      { return timer_; }
   double elapsedInManeuverSec() const { return elapsedSec_; }

private:
   // UNICO ponto de consumo do RNG -- chamado no fim de uma manobra e no
   // reset(), nunca por 'dt'.
   void drawNextInterval();

   bool enabled() const { return stickCommand_ != 0.0; }

   double minIntervalSec_{60.0};
   double maxIntervalSec_{180.0};
   double stickCommand_{0.0};      // 0 = recurso desligado
   double timeoutSec_{20.0};

   Phase phase_{Phase::Idle};
   double timer_{};                // contagem regressiva ate a proxima manobra
   double accumulatedDeg_{};       // rolagem acumulada NESTA manobra (com sinal)
   double lastRollDeg_{};
   bool hasLastRoll_{};
   double elapsedSec_{};           // tempo dentro da manobra atual

   mixr::xrandom::Rng rng_{};   // guarda a propria semente (Rng::reset())
};

} // namespace domain
