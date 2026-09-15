//
// ubf/ObservationBridge.hpp -- toObservation(), extraida do anonimo de
// RLBridgeBehavior.cpp para ficar testavel isoladamente (era copia campo a
// campo a mao, sem a garantia de compilacao que uma macro daria -- o "elo
// mais fraco" do contrato, ver o CHANGELOG.md deste modelo). Este teste e
// o que fecha a lacuna: sem ele, um campo esquecido em toObservation()
// compila e passa em silencio.
//
#include "ubf/ObservationBridge.hpp"

#include <gtest/gtest.h>

namespace {

using namespace mixr::models::xA_4;

// Um WorldView com um valor sentinela DISTINTO por campo -- pega copia
// esquecida (campo fica no default: 0.0/false/"") ou trocada (dois campos
// levando o mesmo valor por engano).
TEST(ObservationBridge, ToObservationCopiaTodosOsCamposNumericosEBooleanos)
{
   domain::WorldView snap;
   snap.northM = 1.0;
   snap.eastM = 2.0;
   snap.altitudeM = 3.0;
   snap.headingDeg = 4.0;
   snap.speedKts = 5.0;
   snap.rollDeg = 6.0;
   snap.pitchDeg = 7.0;
   snap.fuelFraction = 8.0;
   snap.mach = 9.0;
   snap.gLoad = 10.0;
   snap.alphaDeg = 11.0;
   snap.terrainElevM = 12.0;
   snap.altitudeAglM = 13.0;
   snap.contactRangeM = 14.0;
   snap.contactRelBearingDeg = 15.0;
   snap.contactDeltaAltM = 16.0;
   snap.contactNorthM = 17.0;
   snap.contactEastM = 18.0;
   snap.contactAltitudeM = 19.0;
   snap.alertNorthM = 20.0;
   snap.alertEastM = 21.0;
   snap.alertAltitudeM = 22.0;
   snap.alertRangeM = 23.0;
   snap.valid = true;
   snap.terrainValid = false;
   snap.hasContact = true;
   snap.hasAlert = false;
   snap.weaponReady = true;
   // Os 10 campos abaixo ja existiam em WorldView, mas a copia manual
   // antiga nunca os levava para xrlbridge::Observation.
   snap.rwrThreatRangeM = 24.0;
   snap.rwrThreatRelBearingDeg = 25.0;
   snap.rwrThreatDeltaAltM = 26.0;
   snap.hasRwrThreat = true;
   snap.navTrueBrgDeg = 27.0;
   snap.navCmdAltM = 28.0;
   snap.navCmdSpeedKts = 29.0;
   snap.hasNavSteering = false;
   snap.hasNavCmdAlt = true;
   snap.hasNavCmdSpeed = false;

   const auto obs = toObservation(snap);

   EXPECT_DOUBLE_EQ(obs.northM, 1.0);
   EXPECT_DOUBLE_EQ(obs.eastM, 2.0);
   EXPECT_DOUBLE_EQ(obs.altitudeM, 3.0);
   EXPECT_DOUBLE_EQ(obs.headingDeg, 4.0);
   EXPECT_DOUBLE_EQ(obs.speedKts, 5.0);
   EXPECT_DOUBLE_EQ(obs.rollDeg, 6.0);
   EXPECT_DOUBLE_EQ(obs.pitchDeg, 7.0);
   EXPECT_DOUBLE_EQ(obs.fuelFraction, 8.0);
   EXPECT_DOUBLE_EQ(obs.mach, 9.0);
   EXPECT_DOUBLE_EQ(obs.gLoad, 10.0);
   EXPECT_DOUBLE_EQ(obs.alphaDeg, 11.0);
   EXPECT_DOUBLE_EQ(obs.terrainElevM, 12.0);
   EXPECT_DOUBLE_EQ(obs.altitudeAglM, 13.0);
   EXPECT_DOUBLE_EQ(obs.contactRangeM, 14.0);
   EXPECT_DOUBLE_EQ(obs.contactRelBearingDeg, 15.0);
   EXPECT_DOUBLE_EQ(obs.contactDeltaAltM, 16.0);
   EXPECT_DOUBLE_EQ(obs.contactNorthM, 17.0);
   EXPECT_DOUBLE_EQ(obs.contactEastM, 18.0);
   EXPECT_DOUBLE_EQ(obs.contactAltitudeM, 19.0);
   EXPECT_DOUBLE_EQ(obs.alertNorthM, 20.0);
   EXPECT_DOUBLE_EQ(obs.alertEastM, 21.0);
   EXPECT_DOUBLE_EQ(obs.alertAltitudeM, 22.0);
   EXPECT_DOUBLE_EQ(obs.alertRangeM, 23.0);
   EXPECT_TRUE(obs.valid);
   EXPECT_FALSE(obs.terrainValid);
   EXPECT_TRUE(obs.hasContact);
   EXPECT_FALSE(obs.hasAlert);
   EXPECT_TRUE(obs.weaponReady);

   EXPECT_DOUBLE_EQ(obs.rwrThreatRangeM, 24.0);
   EXPECT_DOUBLE_EQ(obs.rwrThreatRelBearingDeg, 25.0);
   EXPECT_DOUBLE_EQ(obs.rwrThreatDeltaAltM, 26.0);
   EXPECT_TRUE(obs.hasRwrThreat);
   EXPECT_DOUBLE_EQ(obs.navTrueBrgDeg, 27.0);
   EXPECT_DOUBLE_EQ(obs.navCmdAltM, 28.0);
   EXPECT_DOUBLE_EQ(obs.navCmdSpeedKts, 29.0);
   EXPECT_FALSE(obs.hasNavSteering);
   EXPECT_TRUE(obs.hasNavCmdAlt);
   EXPECT_FALSE(obs.hasNavCmdSpeed);
}

TEST(ObservationBridge, ToObservationCopiaOsCamposDeTexto)
{
   domain::WorldView snap;
   snap.contactName = "bandit1";
   snap.alertSender = "falcon2";
   snap.alertContactName = "bandit1";
   snap.ownerName = "falcon1";

   const auto obs = toObservation(snap);
   EXPECT_EQ(obs.contactName, "bandit1");
   EXPECT_EQ(obs.alertSender, "falcon2");
   EXPECT_EQ(obs.alertContactName, "bandit1");
   // ownerName identifica de qual player veio a observacao -- ver o
   // comentario em libs/xrlbridge/RLBridge.hpp::Observation::ownerName.
   EXPECT_EQ(obs.ownerName, "falcon1");
}

} // namespace
