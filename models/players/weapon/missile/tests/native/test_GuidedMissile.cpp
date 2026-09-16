//
// xnative/GuidedMissile.cpp -- weaponGuidance()/weaponDynamics(), a camada
// que test_Guidance.cpp (lei de navegacao pura, sem MIXR) nao alcanca.
// Precisa de um Player/WorldModel reais -- mesmo padrao "Bench" ja usado em
// models/players/air/A-4/tests/native/test_flight_state_action.cpp.
//
// O teste mais importante daqui trava a correcao documentada no cabecalho
// de atReleaseInit(): SEM ela, o missil guina para rumo/arfagem GEOGRAFICOS
// ZERO a taxa maxima durante toda a janela do TSG (achado medido, nao
// hipotetico -- ver o comentario do metodo). Os dois testes de contraste
// abaixo (com/sem atReleaseInit()) provam que a correcao continua no lugar
// e que, se ela sumir de novo, este teste denuncia.
//
#include "xnative/GuidedMissile.hpp"

#include "mixr/models/WorldModel.hpp"
#include "mixr/models/player/Player.hpp"

#include "mixr/base/units/angle_utils.hpp"

#include <gtest/gtest.h>

namespace {

using namespace mixr;

// weaponGuidance()/weaponDynamics() sao PROTECTED (chamados em producao so
// pela maquinaria de fase de AbstractWeapon::dynamics()) -- mesma tecnica
// de sonda ja usada para AlertDatalink em models/players/air/A-4.
class SondaGuidedMissile : public models::xmissile::GuidedMissile
{
public:
   using GuidedMissile::weaponDynamics;
   using GuidedMissile::weaponGuidance;
};

// Mesma ideia do 'Bench' de test_flight_state_action.cpp: um WorldModel de
// bancada, so' pra Player::setEulerAngles()/setVelocity() nao operarem
// sobre um objeto sem container.
struct MissileBench
{
   models::WorldModel* const world{new models::WorldModel()};
   SondaGuidedMissile* const missile{new SondaGuidedMissile()};

   MissileBench()
   {
      missile->container(world);
      missile->reset();
   }

   ~MissileBench()
   {
      missile->unref();
      world->unref();
   }
};

} // namespace

TEST(GuidedMissile, ComAtReleaseInitMantemRumoEArfagemDeLancamento)
{
   MissileBench bench;

   // Rumo 90 graus (Leste), arfagem 10 graus, velocidade de lancamento
   // abaixo do maximo (280 m/s) -- geometria arbitraria, so precisa ser
   // longe de zero nos tres eixos pra expor um salto na direcao errada.
   const double headingRad{90.0 * base::angle::D2RCC};
   const double pitchRad{10.0 * base::angle::D2RCC};
   bench.missile->setEulerAngles(0.0, pitchRad, headingRad);
   bench.missile->setVelocity(0.0, 200.0, 0.0);   // NED: 200 m/s para Leste

   bench.missile->atReleaseInit();
   bench.missile->weaponDynamics(0.02);   // um frame de tempo critico

   // atReleaseInit() semeou o comando com a atitude de lancamento, entao o
   // delta de um unico frame tem que ser minusculo -- bem menor que o
   // passo maximo de giro (maxG=15G a 200 m/s, ~0.73 rad/s).
   EXPECT_NEAR(bench.missile->getHeadingR(), headingRad, 0.02);
   EXPECT_NEAR(bench.missile->getPitchR(), pitchRad, 0.02);
}

TEST(GuidedMissile, SemAtReleaseInitGuinaParaRumoGeograficoZero)
{
   MissileBench bench;

   const double headingRad{90.0 * base::angle::D2RCC};
   bench.missile->setEulerAngles(0.0, 0.0, headingRad);
   bench.missile->setVelocity(0.0, 200.0, 0.0);

   // Deliberadamente SEM atReleaseInit() -- cmdHeadingRad_ fica no default
   // de construcao (0.0 rad = Norte geografico). Este teste existe para
   // provar que o cenario que atReleaseInit() evita e' de fato observavel
   // (da confianca de que o teste acima esta medindo a coisa certa, nao um
   // efeito que aconteceria de qualquer forma).
   bench.missile->weaponDynamics(0.02);

   EXPECT_LT(bench.missile->getHeadingR(), headingRad)
      << "sem atReleaseInit(), o rumo deveria comecar a girar em direcao ao Norte (0 rad)";
}

TEST(GuidedMissile, WeaponGuidanceSemAlvoNaoCrasha)
{
   MissileBench bench;
   EXPECT_NO_FATAL_FAILURE(bench.missile->weaponGuidance(0.02));
}

TEST(GuidedMissile, WeaponGuidanceComAlvoInativoNaoCrasha)
{
   MissileBench bench;

   const auto alvo = new models::Player();
   alvo->container(bench.world);
   alvo->reset();
   alvo->setMode(models::Player::INACTIVE);
   bench.missile->setTargetPlayer(alvo, true);

   EXPECT_NO_FATAL_FAILURE(bench.missile->weaponGuidance(0.02));

   alvo->unref();
}
