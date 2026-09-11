#pragma once

#include "xrandom/DeterministicRng.hpp"

#include <cstdint>

namespace domain {

//------------------------------------------------------------------------------
// EvasionReactionPlan -- o ATRASO de reacao do piloto entre PERCEBER uma
// ameaca (o RWR mostra um emissor hostil) e de fato comecar a manobra
// evasiva.
//
// Regra de negocio pura (sem MIXR, sem BehaviorTree.CPP, testavel isolada),
// no MESMO molde de domain::AerobaticPlan: a classe so semeia e sorteia; nao
// sabe de semente mestra, nome de player nem salt de proposito -- a semente
// que chega por setSeed() ja' e' o resultado FINAL da hierarquia de
// derivacao (semente do cenario -> hash do nome do player -> salt de
// proposito), calculada por quem chama (ver BtBehavior::configurePlans() e
// libs/xrandom/DeterministicRng.hpp).
//
// DIFERENCA DELIBERADA em relacao a AerobaticPlan: aqui o sorteio e'
// DISPARADO por um evento EXTERNO (a ameaca aparecer), nao um relogio que
// conta sozinho desde o reset(). AerobaticPlan sorteia o intervalo ate' a
// PROXIMA manobra, sem depender de nada fora dela; EvasionReactionPlan so'
// comeca a contar quando 'hasThreat' vira true pela primeira vez -- antes
// disso fica parada, sem consumir o RNG.
//
// DETERMINISMO -- mesma disciplina de AerobaticPlan/PatrolPlan: o sorteio
// acontece numa UNICA borda discreta (Idle -> Waiting, a primeira vez que a
// ameaca aparece), NUNCA por 'dt'. O numero de vezes que uma aeronave e'
// ameacada nao depende de quantas threads de tempo critico existem, entao
// consumir o RNG so' nessa borda mantem a sequencia identica com 1, 2 e 4
// threads.
//
// SEM CREDITO PARCIAL: se a ameaca desaparecer antes do atraso vencer, o
// progresso e' perdido -- a proxima deteccao sorteia um atraso NOVO, do
// zero. Simplificacao deliberada (ao contrario da manobra de AerobaticPlan,
// que nunca aborta no meio por ja' estar fisicamente comprometida com uma
// rolagem em curso): aqui nao ha nenhum comando de controle ja' aplicado
// enquanto so' se está contando o atraso, entao nao ha "meio" para proteger.
//------------------------------------------------------------------------------
class EvasionReactionPlan
{
public:
   enum class Phase { Idle, Waiting, Reacted };

   EvasionReactionPlan() = default;

   // minDelaySec >= maxDelaySec vira atraso FIXO (sem sorteio), o que
   // mantem o recurso utilizavel de forma totalmente deterministica.
   void configure(double minDelaySec, double maxDelaySec);

   // 'seed' e' o resultado final ja' derivado -- ver o comentario de classe.
   void setSeed(std::uint64_t seed);

   void reset();

   // Avanca a maquina de estados. 'hasThreat' e' a percepcao CRUA deste
   // frame (RWR mostra um emissor hostil agora). Devolve true a partir do
   // instante em que o atraso sorteado vence, enquanto a ameaca continuar
   // presente -- e' o sinal que libera a manobra de verdade (ver
   // ubf::BtBehavior::feedRwrEvasion()).
   bool update(double dt, bool hasThreat);

   bool reacting() const             { return phase_ == Phase::Reacted; }
   double timeToReactSec() const     { return timer_; }

private:
   // UNICO ponto de consumo do RNG -- chamado so' na borda Idle->Waiting.
   void drawNextDelay();

   double minDelaySec_{0.0};
   double maxDelaySec_{0.0};

   Phase phase_{Phase::Idle};
   double timer_{};   // contagem regressiva ate' a reacao

   mixr::xrandom::Rng rng_{};   // guarda a propria semente (Rng::reset())
};

} // namespace domain
