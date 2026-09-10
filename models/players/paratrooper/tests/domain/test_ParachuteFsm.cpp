#include "domain/ParachuteFsm.hpp"

#include <gtest/gtest.h>

namespace mixr::models::xparatrooper::domain {
namespace {

constexpr JumpProfile kProfile{/*deployAglM=*/300.0, /*groundAglM=*/2.0};

TEST(ParachuteFsm, FreefallPermaneceAcimaDoLimiarDeAbertura)
{
   EXPECT_EQ(Stage::FREEFALL, next(Stage::FREEFALL, 301.0, kProfile));
}

TEST(ParachuteFsm, FreefallAbreExatamenteNoLimiar)
{
   // Fronteira INCLUSIVA (<=) -- AGL igual ao limiar ja abre o paraquedas.
   EXPECT_EQ(Stage::CANOPY, next(Stage::FREEFALL, 300.0, kProfile));
}

TEST(ParachuteFsm, FreefallAbreAbaixoDoLimiar)
{
   EXPECT_EQ(Stage::CANOPY, next(Stage::FREEFALL, 299.0, kProfile));
}

TEST(ParachuteFsm, CanopyPermaneceAcimaDoSolo)
{
   EXPECT_EQ(Stage::CANOPY, next(Stage::CANOPY, 3.0, kProfile));
}

TEST(ParachuteFsm, CanopyPousaExatamenteNoLimiarDeSolo)
{
   EXPECT_EQ(Stage::LANDED, next(Stage::CANOPY, 2.0, kProfile));
}

TEST(ParachuteFsm, CanopyPousaComAglNegativa)
{
   // O caminho de "AGL cruzou zero antes do frame seguinte reagir" --
   // xnative::Paratrooper::crashNotification() se apoia nisto ser total.
   EXPECT_EQ(Stage::LANDED, next(Stage::CANOPY, -10.0, kProfile));
}

TEST(ParachuteFsm, CanopyNuncaVoltaParaFreefallMesmoComAglSubindoMuito)
{
   // Mao unica: sobrevoar um vale sob o velame nao "reabre" a queda livre.
   EXPECT_EQ(Stage::CANOPY, next(Stage::CANOPY, 5000.0, kProfile));
}

TEST(ParachuteFsm, LandedEhAbsorvente)
{
   for (const double agl : {-10.0, 0.0, 2.0, 300.0, 5000.0}) {
      EXPECT_EQ(Stage::LANDED, next(Stage::LANDED, agl, kProfile)) << "agl=" << agl;
   }
}

TEST(ParachuteFsm, ConfiguracaoDegeneradaAindaTerminaEmLanded)
{
   // groundAgl >= deployAgl: o "ramo do meio" (CANOPY) pode nao durar nem um
   // frame na pratica, mas a funcao continua TOTAL -- nunca "trava" sem
   // estagio.
   const JumpProfile degenerada{/*deployAglM=*/5.0, /*groundAglM=*/5.0};

   EXPECT_EQ(Stage::FREEFALL, next(Stage::FREEFALL, 10.0, degenerada));
   EXPECT_EQ(Stage::CANOPY, next(Stage::FREEFALL, 5.0, degenerada));
   EXPECT_EQ(Stage::LANDED, next(Stage::CANOPY, 5.0, degenerada));
}

TEST(ParachuteFsm, LabelIdaEVoltaParaOsTresEstagios)
{
   for (const Stage s : {Stage::FREEFALL, Stage::CANOPY, Stage::LANDED}) {
      Stage roundTrip{};
      ASSERT_TRUE(stageFromLabel(labelOf(s), roundTrip));
      EXPECT_EQ(s, roundTrip);
   }
}

TEST(ParachuteFsm, RotuloDesconhecidoFalhaSemLancar)
{
   Stage out{Stage::FREEFALL};
   EXPECT_FALSE(stageFromLabel("BOGUS", out));
   EXPECT_FALSE(stageFromLabel("", out));
   EXPECT_FALSE(stageFromLabel(nullptr, out));
}

TEST(ParachuteFsm, SequenciaCompletaDeUmSaltoTemExatamenteDuasTransicoes)
{
   Stage stage{Stage::FREEFALL};
   int transicoes{};
   Stage anterior{stage};

   for (double agl = 1200.0; agl >= -1.0; agl -= 1.09) {
      stage = next(stage, agl, kProfile);
      if (stage != anterior) ++transicoes;
      anterior = stage;
   }

   EXPECT_EQ(2, transicoes);
   EXPECT_EQ(Stage::LANDED, stage);
}

} // namespace
} // namespace mixr::models::xparatrooper::domain
