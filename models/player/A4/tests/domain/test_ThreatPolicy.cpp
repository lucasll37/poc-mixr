// A manobra de evasao. E o pedaco de regra com mais historia atras: o
// cabecalho de domain/ThreatPolicy.hpp registra tres correcoes que vieram de
// ver as aeronaves "batendo asa" no Tacview (alvo recalculado a cada tick,
// rumo relativo ao proprio nariz, e a falta de histerese). Cada uma delas
// virou um teste aqui -- e o ponto: sem isso, a regressao volta silenciosa e
// so aparece olhando o voo.

#include "domain/ThreatPolicy.hpp"

#include "domain/TerrainFloor.hpp"
#include "domain/geometry.hpp"

#include <gtest/gtest.h>

namespace {

constexpr double TOL{1e-9};
constexpr double MIN_SAFE_ALT_M{200.0};   // igual ao anonimo de ThreatPolicy.cpp

domain::EvasionLimits limitesPadrao()
{
   domain::EvasionLimits l;
   l.breakTurnDeg = 110.0;
   l.climbM = 250.0;
   l.dashSpeedKts = 160.0;
   l.holdSeconds = 30.0;
   l.terrainClearanceM = 500.0;
   return l;
}

domain::ThreatContact contato(const double relBearingDeg, const double deltaAltM,
                              const double rangeM = 8000.0)
{
   domain::ThreatContact c;
   c.rangeM = rangeM;
   c.relBearingDeg = relBearingDeg;
   c.deltaAltM = deltaAltM;
   return c;
}

domain::GroundReference solo(const double elevM)
{
   domain::GroundReference g;
   g.valid = true;
   g.elevationM = elevM;
   return g;
}

const domain::GroundReference SEM_SOLO{};

//------------------------------------------------------------------------------
// Histerese -- o que mata a oscilacao EVADE<->SUPPORT
//------------------------------------------------------------------------------

TEST(ThreatPolicy, SemContatoNemHistereseNaoEstaEngajada)
{
   domain::ThreatPolicy p{limitesPadrao()};
   p.update(1.0, false, contato(0.0, 0.0), 0.0, 3000.0, SEM_SOLO);
   EXPECT_FALSE(p.engaged());
   EXPECT_FALSE(p.contactLive());
}

TEST(ThreatPolicy, ContatoEngajaEArmaOTimerCheio)
{
   domain::ThreatPolicy p{limitesPadrao()};
   p.update(1.0, true, contato(30.0, 100.0), 0.0, 3000.0, SEM_SOLO);
   EXPECT_TRUE(p.engaged());
   EXPECT_TRUE(p.contactLive());
   EXPECT_NEAR(p.holdRemaining(), 30.0, TOL);
}

// A propriedade central: engaged() sobrevive ao contato por holdSeconds
// exatos -- nem um passo a mais, nem um a menos.
TEST(ThreatPolicy, EngajamentoSobreviveExatamenteHoldSeconds)
{
   domain::ThreatPolicy p{limitesPadrao()};
   p.update(1.0, true, contato(30.0, 100.0), 0.0, 3000.0, SEM_SOLO);

   for (int passo = 1; passo <= 29; ++passo) {
      p.update(1.0, false, contato(0.0, 0.0), 0.0, 3000.0, SEM_SOLO);
      EXPECT_TRUE(p.engaged()) << "soltou a manobra cedo demais, no passo " << passo;
      EXPECT_FALSE(p.contactLive()) << "sem pista, contactLive tem de ser falso";
      EXPECT_NEAR(p.holdRemaining(), 30.0 - passo, TOL);
   }

   p.update(1.0, false, contato(0.0, 0.0), 0.0, 3000.0, SEM_SOLO);
   EXPECT_FALSE(p.engaged()) << "a manobra tinha de terminar no 30o segundo";
}

TEST(ThreatPolicy, ReaquisicaoDuranteOArrastoRearmaOTimer)
{
   domain::ThreatPolicy p{limitesPadrao()};
   p.update(1.0, true, contato(30.0, 100.0), 0.0, 3000.0, SEM_SOLO);
   for (int i = 0; i < 20; ++i) p.update(1.0, false, contato(0.0, 0.0), 0.0, 3000.0, SEM_SOLO);
   EXPECT_NEAR(p.holdRemaining(), 10.0, TOL);

   p.update(1.0, true, contato(45.0, -50.0), 0.0, 3000.0, SEM_SOLO);
   EXPECT_NEAR(p.holdRemaining(), 30.0, TOL) << "ver o contato de novo tem de reencher o timer";
}

//------------------------------------------------------------------------------
// Alvo fixado na ENTRADA -- a correcao que fez a curva terminar
//------------------------------------------------------------------------------

TEST(ThreatPolicy, AlvoEhFixadoNaEntradaENaoSegueOContato)
{
   domain::ThreatPolicy p{limitesPadrao()};
   p.update(1.0, true, contato(30.0, 100.0), 0.0, 3000.0, SEM_SOLO);
   const domain::FlightCommand fixado{p.command()};

   // o intruso continua manobrando e a propria aeronave girou: nada disso
   // pode mover o alvo enquanto a manobra estiver valendo
   for (double bearing = -180.0; bearing <= 180.0; bearing += 15.0) {
      p.update(1.0, true, contato(bearing, -200.0), bearing / 2.0, 2500.0, solo(900.0));
      EXPECT_NEAR(p.command().headingDeg, fixado.headingDeg, TOL);
      EXPECT_NEAR(p.command().altitudeM, fixado.altitudeM, TOL);
      EXPECT_NEAR(p.command().speedKts, fixado.speedKts, TOL);
   }
}

TEST(ThreatPolicy, ReengajarDepoisDeExpirarRecalculaOAlvo)
{
   domain::ThreatPolicy p{limitesPadrao()};
   p.update(1.0, true, contato(30.0, 100.0), 0.0, 3000.0, SEM_SOLO);
   const double primeiro{p.command().headingDeg};

   for (int i = 0; i < 30; ++i) p.update(1.0, false, contato(0.0, 0.0), 0.0, 3000.0, SEM_SOLO);
   ASSERT_FALSE(p.engaged());

   p.update(1.0, true, contato(-60.0, 100.0), 0.0, 3000.0, SEM_SOLO);
   EXPECT_TRUE(p.engaged());
   EXPECT_NE(p.command().headingDeg, primeiro) << "manobra nova tem de ter alvo novo";
}

//------------------------------------------------------------------------------
// Para onde fugir -- rumo relativo ao CONTATO, nao ao proprio nariz
//------------------------------------------------------------------------------

TEST(ThreatPolicy, ContatoABoresteQuebraParaBombordo)
{
   domain::ThreatPolicy p{limitesPadrao()};
   // nariz ao norte, contato 30 graus a boreste => marcacao absoluta 30
   // fuga = 30 - 110 = -80 => 280
   p.update(1.0, true, contato(30.0, 100.0), 0.0, 3000.0, SEM_SOLO);
   EXPECT_NEAR(p.command().headingDeg, 280.0, TOL);
}

TEST(ThreatPolicy, ContatoABombordoQuebraParaBoreste)
{
   domain::ThreatPolicy p{limitesPadrao()};
   // nariz ao norte, contato 30 graus a bombordo => marcacao absoluta 330
   // fuga = 330 + 110 = 440 => 80
   p.update(1.0, true, contato(-30.0, 100.0), 0.0, 3000.0, SEM_SOLO);
   EXPECT_NEAR(p.command().headingDeg, 80.0, TOL);
}

// A segunda correcao do cabecalho: o alvo sai da marcacao do CONTATO. Com o
// mesmo contato relativo e narizes diferentes, os rumos de fuga tem de
// diferir exatamente pela diferenca dos narizes.
TEST(ThreatPolicy, RumoDeFugaAcompanhaAMarcacaoAbsolutaDoContato)
{
   domain::ThreatPolicy a{limitesPadrao()};
   a.update(1.0, true, contato(45.0, 0.0), 0.0, 3000.0, SEM_SOLO);

   domain::ThreatPolicy b{limitesPadrao()};
   b.update(1.0, true, contato(45.0, 0.0), 90.0, 3000.0, SEM_SOLO);

   const double esperado{domain::wrap360(a.command().headingDeg + 90.0)};
   EXPECT_NEAR(b.command().headingDeg, esperado, TOL);
}

//------------------------------------------------------------------------------
// Desconflito vertical e o piso
//------------------------------------------------------------------------------

TEST(ThreatPolicy, DesconflitaNoSentidoOpostoAoDoContato)
{
   domain::ThreatPolicy p{limitesPadrao()};
   p.update(1.0, true, contato(30.0, 400.0), 0.0, 3000.0, SEM_SOLO);   // contato ACIMA
   EXPECT_NEAR(p.command().altitudeM, 2750.0, TOL) << "contato acima => desce";

   domain::ThreatPolicy q{limitesPadrao()};
   q.update(1.0, true, contato(30.0, -400.0), 0.0, 3000.0, SEM_SOLO);  // contato ABAIXO
   EXPECT_NEAR(q.command().altitudeM, 3250.0, TOL) << "contato abaixo => sobe";
}

TEST(ThreatPolicy, DescidaNuncaFuraOPisoDeTerreno)
{
   domain::ThreatPolicy p{limitesPadrao()};
   // desceria para 750, mas o terreno a 1500 com folga de 500 poe o piso em 2000
   p.update(1.0, true, contato(30.0, 400.0), 0.0, 1000.0, solo(1500.0));
   EXPECT_NEAR(p.command().altitudeM, 2000.0, TOL);
}

TEST(ThreatPolicy, SemTerrenoValidoAindaHaOPisoAbsoluto)
{
   domain::ThreatPolicy p{limitesPadrao()};
   p.update(1.0, true, contato(30.0, 400.0), 0.0, 300.0, SEM_SOLO);   // desceria para 50
   EXPECT_NEAR(p.command().altitudeM, MIN_SAFE_ALT_M, TOL);
}

// Borda nao coberta ate aqui: deltaAltM == 0.0 cai no ramo ">= 0.0" de
// breakCommand() (mesma convencao de contato "acima"), logo desce -- e nao um
// caso ambiguo tratado por acaso.
TEST(ThreatPolicy, ContatoNaMesmaAltitudeContaComoAcimaEDesce)
{
   domain::ThreatPolicy p{limitesPadrao()};
   p.update(1.0, true, contato(30.0, 0.0), 0.0, 3000.0, SEM_SOLO);
   EXPECT_NEAR(p.command().altitudeM, 2750.0, TOL);
}

TEST(ThreatPolicy, VelocidadeDaManobraEhSempreADeArranque)
{
   domain::ThreatPolicy p{limitesPadrao()};
   p.update(1.0, true, contato(30.0, 100.0), 0.0, 3000.0, SEM_SOLO);
   EXPECT_NEAR(p.command().speedKts, 160.0, TOL);
}

// O invariante que a poc inteira existe para nao violar: nao ha combinacao de
// entrada em que a manobra comande altitude abaixo do piso vigente.
TEST(ThreatPolicy, InvarianteAltitudeComandadaNuncaFuraOPiso)
{
   const domain::EvasionLimits lim{limitesPadrao()};

   for (double elev = 0.0; elev <= 2500.0; elev += 500.0) {
      for (const bool temSolo : {false, true}) {
         for (double alt = 250.0; alt <= 4000.0; alt += 250.0) {
            for (double bearing = -180.0; bearing <= 180.0; bearing += 30.0) {
               for (const double dAlt : {-500.0, 0.0, 500.0}) {
                  domain::GroundReference g;
                  g.valid = temSolo;
                  g.elevationM = elev;

                  domain::ThreatPolicy p{lim};
                  p.update(1.0, true, contato(bearing, dAlt), 0.0, alt, g);

                  const double piso{
                     domain::terrainFloorM(g, lim.terrainClearanceM, MIN_SAFE_ALT_M)};

                  EXPECT_GE(p.command().altitudeM, piso)
                     << "elev=" << elev << " solo=" << temSolo << " alt=" << alt
                     << " bearing=" << bearing << " dAlt=" << dAlt;

                  const double h{p.command().headingDeg};
                  EXPECT_GE(h, 0.0);
                  EXPECT_LT(h, 360.0) << "rumo comandado fora de [0,360)";
               }
            }
         }
      }
   }
}

TEST(ThreatPolicy, ResetLimpaTudo)
{
   domain::ThreatPolicy p{limitesPadrao()};
   p.update(1.0, true, contato(30.0, 100.0), 0.0, 3000.0, SEM_SOLO);
   ASSERT_TRUE(p.engaged());

   p.reset();
   EXPECT_FALSE(p.engaged());
   EXPECT_FALSE(p.contactLive());
   EXPECT_NEAR(p.holdRemaining(), 0.0, TOL);
}

} // namespace
