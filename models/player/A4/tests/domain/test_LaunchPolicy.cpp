// domain::inLaunchEnvelope() -- as duas perguntas independentes (alcance e
// cone) e as bordas entre elas. Ver o "porque" em domain/LaunchPolicy.hpp.

#include "domain/LaunchPolicy.hpp"

#include <gtest/gtest.h>

namespace {

const domain::LaunchEnvelope kEnv{1500.0, 9000.0, 45.0};

TEST(LaunchPolicy, DentroDoAlcanceEDoConeEhVerdadeiro)
{
   EXPECT_TRUE(domain::inLaunchEnvelope(kEnv, 5000.0, 0.0));
   EXPECT_TRUE(domain::inLaunchEnvelope(kEnv, 5000.0, 30.0));
   EXPECT_TRUE(domain::inLaunchEnvelope(kEnv, 5000.0, -30.0));
}

TEST(LaunchPolicy, AbaixoDoAlcanceMinimoFalha)
{
   EXPECT_FALSE(domain::inLaunchEnvelope(kEnv, 1000.0, 0.0));
}

TEST(LaunchPolicy, AcimaDoAlcanceMaximoFalha)
{
   EXPECT_FALSE(domain::inLaunchEnvelope(kEnv, 9500.0, 0.0));
}

TEST(LaunchPolicy, ForaDoConeFalhaDosDoisLados)
{
   EXPECT_FALSE(domain::inLaunchEnvelope(kEnv, 5000.0, 46.0));
   EXPECT_FALSE(domain::inLaunchEnvelope(kEnv, 5000.0, -46.0));
}

TEST(LaunchPolicy, BordasSaoInclusivas)
{
   EXPECT_TRUE(domain::inLaunchEnvelope(kEnv, 1500.0, 45.0));
   EXPECT_TRUE(domain::inLaunchEnvelope(kEnv, 9000.0, -45.0));
}

} // namespace
