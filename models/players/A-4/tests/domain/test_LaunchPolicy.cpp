#include "domain/LaunchPolicy.hpp"

#include <gtest/gtest.h>

namespace domain {
namespace {

TEST(LaunchPolicy, DentroDoEnvelopeQuandoAlcanceEMarcacaoEstaoNaFaixa)
{
   const LaunchEnvelope env{500.0, 9000.0, 45.0};
   EXPECT_TRUE(inLaunchEnvelope(env, 5000.0, 10.0));
   EXPECT_TRUE(inLaunchEnvelope(env, 5000.0, -30.0));
}

TEST(LaunchPolicy, ForaDoEnvelopePorAlcanceCurtoDemais)
{
   const LaunchEnvelope env{500.0, 9000.0, 45.0};
   EXPECT_FALSE(inLaunchEnvelope(env, 400.0, 0.0));
}

TEST(LaunchPolicy, ForaDoEnvelopePorAlcanceLongoDemais)
{
   const LaunchEnvelope env{500.0, 9000.0, 45.0};
   EXPECT_FALSE(inLaunchEnvelope(env, 9500.0, 0.0));
}

TEST(LaunchPolicy, ForaDoConeDeLancamento)
{
   const LaunchEnvelope env{500.0, 9000.0, 45.0};
   EXPECT_FALSE(inLaunchEnvelope(env, 5000.0, 60.0));
   EXPECT_FALSE(inLaunchEnvelope(env, 5000.0, -60.0));
}

TEST(LaunchPolicy, BordasDoAlcanceSaoInclusivas)
{
   const LaunchEnvelope env{500.0, 9000.0, 45.0};
   EXPECT_TRUE(inLaunchEnvelope(env, 500.0, 0.0));
   EXPECT_TRUE(inLaunchEnvelope(env, 9000.0, 0.0));
}

TEST(LaunchPolicy, BordaDoConeEInclusiva)
{
   const LaunchEnvelope env{500.0, 9000.0, 45.0};
   EXPECT_TRUE(inLaunchEnvelope(env, 5000.0, 45.0));
   EXPECT_TRUE(inLaunchEnvelope(env, 5000.0, -45.0));
}

} // namespace
} // namespace domain
