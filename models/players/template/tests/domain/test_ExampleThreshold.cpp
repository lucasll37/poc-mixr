#include "domain/ExampleThreshold.hpp"

#include <gtest/gtest.h>

namespace mixr::models::xtemplate::domain {
namespace {

TEST(ExampleThreshold, EngajaAoSubirAteOLimiarDeLigar)
{
   const ExampleThreshold rule{/*onValue=*/10.0, /*offValue=*/5.0};

   EXPECT_FALSE(rule.next(9.9, /*engaged=*/false));
   EXPECT_TRUE(rule.next(10.0, /*engaged=*/false));
}

TEST(ExampleThreshold, NaoEngajaEntreOsDoisLimiares)
{
   const ExampleThreshold rule{10.0, 5.0};

   // Subiu, mas nao alcancou onValue -- sem histerese nenhuma para
   // "puxar" ainda, o estado continua desengajado.
   EXPECT_FALSE(rule.next(7.0, /*engaged=*/false));
}

TEST(ExampleThreshold, MantemEngajadoAteCairAbaixoDoLimiarDeDesligar)
{
   const ExampleThreshold rule{10.0, 5.0};

   EXPECT_TRUE(rule.next(7.0, /*engaged=*/true));    // entre os limiares: mantem
   EXPECT_TRUE(rule.next(5.0, /*engaged=*/true));     // no limiar exato: ainda mantem (>=)
   EXPECT_FALSE(rule.next(4.9, /*engaged=*/true));    // abaixo: desliga
}

TEST(ExampleThreshold, LimiaresIguaisDegeneramParaUmLimiarSimples)
{
   const ExampleThreshold rule{5.0, 5.0};

   EXPECT_FALSE(rule.next(4.9, /*engaged=*/false));
   EXPECT_TRUE(rule.next(5.0, /*engaged=*/false));
   EXPECT_FALSE(rule.next(4.9, /*engaged=*/true));
}

} // namespace
} // namespace mixr::models::xtemplate::domain
