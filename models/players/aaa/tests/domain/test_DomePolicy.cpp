#include "domain/DomePolicy.hpp"

#include <gtest/gtest.h>

namespace mixr::models::xaaa::domain {
namespace {

TEST(DomePolicy, DentroDoIntervaloEstaNoDomo)
{
   const Dome dome{/*minRangeM=*/500.0, /*maxRangeM=*/5000.0};

   EXPECT_TRUE(inDome(dome, 500.0));    // borda minima, inclusiva
   EXPECT_TRUE(inDome(dome, 2500.0));   // meio do intervalo
   EXPECT_TRUE(inDome(dome, 5000.0));   // borda maxima, inclusiva
}

TEST(DomePolicy, ForaDoDomoPorAlcanceCurtoDemais)
{
   const Dome dome{500.0, 5000.0};

   EXPECT_FALSE(inDome(dome, 499.9));
   EXPECT_FALSE(inDome(dome, 0.0));
}

TEST(DomePolicy, ForaDoDomoPorAlcanceLongoDemais)
{
   const Dome dome{500.0, 5000.0};

   EXPECT_FALSE(inDome(dome, 5000.1));
   EXPECT_FALSE(inDome(dome, 1e6));
}

// min > max e' um domo degenerado (config invalida no .edl) -- nenhum
// alcance cai dentro dele, o que e' o comportamento seguro (nunca dispara
// em vez de disparar sempre).
TEST(DomePolicy, IntervaloInvalidoNuncaContemNada)
{
   const Dome dome{5000.0, 500.0};

   EXPECT_FALSE(inDome(dome, 0.0));
   EXPECT_FALSE(inDome(dome, 1000.0));
   EXPECT_FALSE(inDome(dome, 1e6));
}

} // namespace
} // namespace mixr::models::xaaa::domain
