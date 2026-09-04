// A derivacao de sementes de shared/xrandom -- so as duas funcoes puras
// (fnv1a64/deriveSeed). O gerador de verdade (std::mt19937_64) mora dentro de
// cada consumidor (ex.: domain::PatrolPlan, testado em models/flight/tests/
// domain/test_PatrolPlan.cpp) -- ver o "porque" no cabecalho de
// DeterministicRng.hpp: domain/ e bt/ do modelo sao compilados sem o SDK, e
// este header so fica visivel via dist/include (publicado pelo SDK).
//
// Este arquivo nao inclui nada do MIXR, de proposito -- mesmo movimento do
// test_xmsg_rules.cpp ao lado.

#include "xrandom/DeterministicRng.hpp"

#include <gtest/gtest.h>

namespace {

using mixr::xrandom::deriveSeed;
using mixr::xrandom::fnv1a64;

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

} // namespace
