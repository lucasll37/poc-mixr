// domain::EvasionReactionPlan -- o atraso de reacao do piloto entre
// PERCEBER uma ameaca de RWR e comecar a evadir.
//
// Regra pura: sem MIXR, sem BehaviorTree.CPP, sem Station. A semente entra
// pronta (a hierarquia de derivacao que a produz e' de BtBehavior::
// configurePlans()).

#include "domain/EvasionReactionPlan.hpp"

#include <gtest/gtest.h>

#include <vector>

namespace {

constexpr double kDt{0.02};   // 50 Hz, o passo de decisao de producao

domain::EvasionReactionPlan fazerPlano(const double minSec, const double maxSec,
                                       const std::uint64_t semente)
{
   domain::EvasionReactionPlan plan;
   plan.configure(minSec, maxSec);
   plan.setSeed(semente);
   return plan;
}

// Roda com a ameaca presente ate' reacting() virar true, devolvendo o tempo
// decorrido. Devolve -1 se nao reagir dentro do limite.
double tempoAteReagir(domain::EvasionReactionPlan& plan, const double limiteSec)
{
   double t{};
   while (t < limiteSec) {
      const bool reacting{plan.update(kDt, /*hasThreat=*/true)};
      t += kDt;
      if (reacting) return t;
   }
   return -1.0;
}

//------------------------------------------------------------------------------
// Sem ameaca, o plano nunca sai de Idle -- nunca consome o RNG, nunca reage.
//------------------------------------------------------------------------------
TEST(EvasionReactionPlan, SemAmeacaNuncaReage)
{
   auto plan = fazerPlano(1.0, 5.0, 42);
   for (int i = 0; i < 5000; ++i) {
      const bool reacting{plan.update(kDt, /*hasThreat=*/false)};
      ASSERT_FALSE(reacting) << "reagiu sem nenhuma ameaca, no passo " << i;
   }
   EXPECT_FALSE(plan.reacting());
}

TEST(EvasionReactionPlan, NaoReageAntesDoAtrasoMinimo)
{
   auto plan = fazerPlano(2.0, 4.0, 7);
   double t{};
   for (int i = 0; i < 90; ++i) {   // 1.8 s -- abaixo do piso de 2.0 s
      const bool reacting{plan.update(kDt, /*hasThreat=*/true)};
      t += kDt;
      ASSERT_FALSE(reacting) << "reagiu cedo demais em t=" << t;
   }
}

TEST(EvasionReactionPlan, ReageDentroDaFaixaConfigurada)
{
   auto plan = fazerPlano(2.0, 4.0, 7);
   const double t{tempoAteReagir(plan, 10.0)};
   ASSERT_GT(t, 0.0) << "nunca reagiu";
   EXPECT_GE(t, 2.0);
   EXPECT_LE(t, 4.0 + kDt);
   EXPECT_TRUE(plan.reacting());
}

//------------------------------------------------------------------------------
// Uma vez reagindo, continua reagindo enquanto a ameaca persistir -- e' o
// sinal que a arvore consulta (RwrThreatDetectedCondition).
//------------------------------------------------------------------------------
TEST(EvasionReactionPlan, ContinuaReagindoEnquantoAAmeacaPersiste)
{
   auto plan = fazerPlano(0.5, 0.5, 1);
   ASSERT_GT(tempoAteReagir(plan, 5.0), 0.0);
   for (int i = 0; i < 200; ++i) {
      ASSERT_TRUE(plan.update(kDt, /*hasThreat=*/true)) << "parou de reagir no passo " << i;
   }
}

//------------------------------------------------------------------------------
// SEM CREDITO PARCIAL -- a ameaca some antes do atraso vencer, o progresso e
// perdido. A proxima deteccao sorteia um atraso NOVO, do zero (nao continua
// de onde parou).
//------------------------------------------------------------------------------
TEST(EvasionReactionPlan, AmeacaSomeAntesDoAtrasoVencerNaoDeixaCreditoParcial)
{
   auto plan = fazerPlano(5.0, 5.0, 1);   // atraso fixo de 5 s

   // Conta quase ate' vencer (4 s de 5).
   for (int i = 0; i < 200; ++i) {
      ASSERT_FALSE(plan.update(kDt, /*hasThreat=*/true));
   }
   ASSERT_FALSE(plan.reacting());

   // A ameaca some por um instante -- reseta.
   EXPECT_FALSE(plan.update(kDt, /*hasThreat=*/false));

   // Reaparece: tem que contar os 5 s INTEIROS de novo, nao só o 1 s que faltava.
   double t{};
   bool reagiu{false};
   for (int i = 0; i < 300 && !reagiu; ++i) {
      reagiu = plan.update(kDt, /*hasThreat=*/true);
      t += kDt;
   }
   ASSERT_TRUE(reagiu);
   EXPECT_NEAR(t, 5.0, kDt);
}

TEST(EvasionReactionPlan, ReagirEDepoisPerderAAmeacaVoltaAoIdle)
{
   auto plan = fazerPlano(0.0, 0.0, 1);
   ASSERT_GT(tempoAteReagir(plan, 5.0), 0.0);
   ASSERT_TRUE(plan.reacting());

   plan.update(kDt, /*hasThreat=*/false);
   EXPECT_FALSE(plan.reacting());
}

//------------------------------------------------------------------------------
// Reprodutibilidade -- a propriedade que libs/xrandom existe para garantir.
//------------------------------------------------------------------------------
std::vector<double> primeirosAtrasos(const std::uint64_t semente, const int quantos)
{
   auto plan = fazerPlano(1.0, 8.0, semente);
   std::vector<double> saida;
   for (int i = 0; i < quantos; ++i) {
      const double t{tempoAteReagir(plan, 20.0)};
      saida.push_back(t);
      plan.update(kDt, /*hasThreat=*/false);   // solta a ameaca para o proximo ciclo
   }
   return saida;
}

TEST(EvasionReactionPlan, MesmaSementeDaSequenciaIdentica)
{
   EXPECT_EQ(primeirosAtrasos(20260911, 4), primeirosAtrasos(20260911, 4));
}

TEST(EvasionReactionPlan, SementesDiferentesDaoSequenciasDiferentes)
{
   EXPECT_NE(primeirosAtrasos(20260911, 4), primeirosAtrasos(1, 4));
}

//------------------------------------------------------------------------------
// max <= min vira atraso FIXO, sem consumir o gerador.
//------------------------------------------------------------------------------
TEST(EvasionReactionPlan, AtrasoFixoNaoDependeDaSemente)
{
   auto a = fazerPlano(3.0, 3.0, 1);
   auto b = fazerPlano(3.0, 3.0, 999);
   const double ta{tempoAteReagir(a, 10.0)};
   const double tb{tempoAteReagir(b, 10.0)};
   EXPECT_NEAR(ta, 3.0, kDt);
   EXPECT_DOUBLE_EQ(ta, tb);
}

TEST(EvasionReactionPlan, MaxMenorQueMinNaoQuebra)
{
   auto plan = fazerPlano(4.0, 1.0, 1);   // invertido de proposito
   const double t{tempoAteReagir(plan, 10.0)};
   ASSERT_GT(t, 0.0);
   EXPECT_NEAR(t, 4.0, kDt);   // colapsa para o piso, nao para uma faixa invalida
}

//------------------------------------------------------------------------------
// dt grande nao deixa reagir cedo demais nem o relogio negativo.
//------------------------------------------------------------------------------
TEST(EvasionReactionPlan, PassoGrandeReageDeUmaVezSemFicarNegativo)
{
   auto plan = fazerPlano(3.0, 3.0, 1);
   EXPECT_FALSE(plan.update(kDt, /*hasThreat=*/true));   // Idle -> Waiting, sorteia 3.0s
   EXPECT_TRUE(plan.update(300.0, /*hasThreat=*/true));  // dt gigante vence de uma vez
   EXPECT_TRUE(plan.reacting());
   EXPECT_GE(plan.timeToReactSec(), 0.0);
}

//------------------------------------------------------------------------------
// Atraso 0/0 (o default de BtTuning) reage no PROPRIO frame em que a ameaca
// aparece -- o sorteio (que devolve o piso sem consumir o gerador, ver
// libs/xrandom::Rng::uniform()) e o decremento acontecem no MESMO update(),
// entao "atraso zero" significa zero segundos de verdade, nao um tick extra
// de graca so por causa da borda de transicao Idle->Waiting.
//------------------------------------------------------------------------------
TEST(EvasionReactionPlan, AtrasoZeroReageNoProprioFrame)
{
   auto plan = fazerPlano(0.0, 0.0, 1);
   EXPECT_TRUE(plan.update(kDt, /*hasThreat=*/true));
   EXPECT_TRUE(plan.reacting());
}

} // namespace
