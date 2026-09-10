// libs/xrandom -- as DUAS camadas: a derivacao de sementes (fnv1a64/
// deriveSeed, puras) e o GERADOR (a classe Rng). Nenhuma outra classe deste
// repositorio instancia um gerador; os consumidores (domain::PatrolPlan,
// domain::AerobaticPlan) guardam um Rng e sao testados a parte, em
// models/players/A-4/tests/domain/.
//
// Este arquivo nao inclui nada do MIXR, de proposito -- mesmo movimento do
// test_xmsg_rules.cpp ao lado.

#include "xrandom/DeterministicRng.hpp"

#include <gtest/gtest.h>

#include <vector>

namespace {

using mixr::xrandom::deriveSeed;
using mixr::xrandom::fnv1a64;
using mixr::xrandom::Rng;

TEST(Fnv1a64, EhEstavelParaAMesmaString)
{
   EXPECT_EQ(fnv1a64("falcon1"), fnv1a64("falcon1"));
   EXPECT_EQ(fnv1a64(""), fnv1a64(""));
}

TEST(Fnv1a64, NomesDosQuatroFalconsNaoColidem)
{
   const auto h1 = fnv1a64("falcon1");
   const auto h2 = fnv1a64("falcon2");
   const auto h3 = fnv1a64("falcon3");
   const auto h4 = fnv1a64("falcon4");

   EXPECT_NE(h1, h2);
   EXPECT_NE(h1, h3);
   EXPECT_NE(h1, h4);
   EXPECT_NE(h2, h3);
   EXPECT_NE(h2, h4);
   EXPECT_NE(h3, h4);
}

TEST(DeriveSeed, EhEstavelParaOMesmoParSeedSalt)
{
   EXPECT_EQ(deriveSeed(42, 7), deriveSeed(42, 7));
}

TEST(DeriveSeed, SaltsDiferentesDaoSequenciasIndependentes)
{
   const std::uint64_t instanceSeed{deriveSeed(20260903, fnv1a64("falcon1"))};

   constexpr std::uint64_t saltA{0x50415452'4F4C4A00ULL}; // "patrol jitter"
   constexpr std::uint64_t saltB{0xAA55AA55'AA55AA55ULL}; // um segundo consumidor qualquer

   EXPECT_NE(deriveSeed(instanceSeed, saltA), deriveSeed(instanceSeed, saltB));
}

TEST(DeriveSeed, SementesDeInstanciaDiferentesPorNomeAindaQueOMasterSejaOMesmo)
{
   constexpr std::uint64_t masterSeed{20260903};

   const auto seedFalcon1 = deriveSeed(masterSeed, fnv1a64("falcon1"));
   const auto seedFalcon2 = deriveSeed(masterSeed, fnv1a64("falcon2"));

   EXPECT_NE(seedFalcon1, seedFalcon2);
}

// A propriedade que 'make check-patrol-seed-*' trava em ponta a ponta (dois
// masterSeed diferentes divergem, mesmo falcon): aqui travada na unidade, sem
// precisar subir Station nenhuma. E o que faz 'patrolMasterSeed' no .edl
// significar alguma coisa -- se dois masters colidissem no mesmo player, o
// slot seria decorativo.
TEST(DeriveSeed, MastersDiferentesDaoSementesDiferentesParaOMesmoPlayer)
{
   const auto salt = fnv1a64("falcon1");
   EXPECT_NE(deriveSeed(20260903, salt), deriveSeed(1, salt));
}

// Overflow e comportamento DEFINIDO (aritmetica modular em uint64_t), nao
// risco -- confirmado nos dois extremos da faixa, que sao exatamente os
// valores mais propensos a expor um overflow tratado por acidente como UB.
TEST(DeriveSeed, ExtremosDaFaixaDeUint64NaoQuebram)
{
   constexpr std::uint64_t maxU64{0xFFFFFFFFFFFFFFFFULL};
   EXPECT_EQ(deriveSeed(maxU64, maxU64), deriveSeed(maxU64, maxU64));
   EXPECT_NE(deriveSeed(maxU64, maxU64), deriveSeed(0, 0));
}

// fnv1a64 nao e so "estavel" -- strings de mesmo comprimento e conteudo
// parecido (o caso realista: falconN) nao podem colidir nem nos bits baixos,
// que e a parte que mais importa para quem consome com '% N' pequeno.
TEST(Fnv1a64, PrefixoComumNaoColide)
{
   EXPECT_NE(fnv1a64("falcon1"), fnv1a64("falcon10"));
   EXPECT_NE(fnv1a64("bandit1"), fnv1a64("falcon1"));
}

//------------------------------------------------------------------------------
// O GERADOR (Rng). As propriedades abaixo sao as que todo o determinismo
// deste repositorio assume -- e que estavam implicitas enquanto cada
// consumidor carregava o proprio std::mt19937_64.
//------------------------------------------------------------------------------
std::vector<double> sortear(Rng& rng, const int quantos)
{
   std::vector<double> saida;
   saida.reserve(quantos);
   for (int i = 0; i < quantos; ++i) saida.push_back(rng.uniform(0.0, 1.0));
   return saida;
}

TEST(Rng, MesmaSementeDaMesmaSequencia)
{
   Rng a{12345};
   Rng b{12345};
   EXPECT_EQ(sortear(a, 20), sortear(b, 20));
}

TEST(Rng, SementesDiferentesDaoSequenciasDiferentes)
{
   Rng a{12345};
   Rng b{12346};
   EXPECT_NE(sortear(a, 20), sortear(b, 20));
}

// A propriedade que motivou a distribuicao ser construida A CADA CHAMADA em
// vez de guardada: uma std::uniform_real_distribution guardada tem estado
// proprio em algumas implementacoes, e esse estado sobreviveria ao seed() --
// resemear nao voltaria ao inicio. Aqui volta.
TEST(Rng, ResemearVoltaAoInicioDaSequencia)
{
   Rng rng{999};
   const auto primeira = sortear(rng, 10);
   sortear(rng, 5);            // avanca o gerador
   rng.seed(999);
   EXPECT_EQ(sortear(rng, 10), primeira);
}

TEST(Rng, SeedValueDevolveASementeEmVigor)
{
   Rng rng{7};
   EXPECT_EQ(rng.seedValue(), 7u);
   rng.seed(42);
   EXPECT_EQ(rng.seedValue(), 42u);
}

TEST(Rng, RecemConstruidoTemSementeZeroEReprodutivel)
{
   Rng a;
   Rng b;
   EXPECT_EQ(a.seedValue(), 0u);
   EXPECT_EQ(sortear(a, 10), sortear(b, 10));
}

TEST(Rng, UniformFicaDentroDaFaixa)
{
   Rng rng{5};
   for (int i = 0; i < 1000; ++i) {
      const double v{rng.uniform(-3.0, 7.0)};
      EXPECT_GE(v, -3.0);
      EXPECT_LT(v, 7.0);
   }
}

// Faixa degenerada devolve o piso SEM consumir o gerador -- e' o que deixa
// domain::AerobaticPlan alternar entre intervalo fixo e sorteado sem
// deslocar a sequencia (e o que permite escrever a chamada sem um 'if').
TEST(Rng, FaixaDegeneradaDevolveOPisoSemConsumirOGerador)
{
   Rng rng{5};
   Rng controle{5};

   EXPECT_DOUBLE_EQ(rng.uniform(4.0, 4.0), 4.0);
   EXPECT_DOUBLE_EQ(rng.uniform(9.0, 2.0), 9.0);   // invertida, tambem degenerada

   EXPECT_EQ(sortear(rng, 10), sortear(controle, 10));
}

TEST(Rng, SymmetricFicaDentroDaAmplitude)
{
   Rng rng{11};
   for (int i = 0; i < 1000; ++i) {
      const double v{rng.symmetric(6.0)};
      EXPECT_GE(v, -6.0);
      EXPECT_LT(v, 6.0);
   }
}

// Amplitude nao-positiva e' como um consumidor DESLIGA a variacao. Tem de
// devolver zero (nao um numero positivo, que era o risco de delegar direto a
// uniform(-a, a) com a negativo) e nao pode consumir o gerador.
TEST(Rng, SymmetricComAmplitudeNaoPositivaDaZeroSemConsumir)
{
   Rng rng{5};
   Rng controle{5};

   EXPECT_DOUBLE_EQ(rng.symmetric(0.0), 0.0);
   EXPECT_DOUBLE_EQ(rng.symmetric(-2.5), 0.0);

   EXPECT_EQ(sortear(rng, 10), sortear(controle, 10));
}

// A hierarquia inteira, ponta a ponta: e' assim que BtBehavior::
// configurePlans() semeia cada consumidor de cada player.
TEST(Rng, DoisConsumidoresDoMesmoPlayerNaoCorrelacionam)
{
   const auto instanceSeed = deriveSeed(20260910, fnv1a64("a4_3"));
   Rng patrulha{deriveSeed(instanceSeed, 0x5041'5452'4F4C'4A00ULL)};
   Rng acrobacia{deriveSeed(instanceSeed, 0x524F'4C4C'5341'4C54ULL)};
   EXPECT_NE(sortear(patrulha, 20), sortear(acrobacia, 20));
}

} // namespace
