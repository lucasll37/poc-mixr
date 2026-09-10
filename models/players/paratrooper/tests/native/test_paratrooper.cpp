//
// Camada 3 do MODELO -- as classes MIXR proprias, SEM levantar Station.
//
// 'BenchWorld' e' o UNICO WorldModel que estes testes levantam, e so' para
// dar a Paratrooper::updateTC() o 'getWorldModel()->phase()' de que ela
// depende (AbstractWeapon::updateTC() le a fase direto do WorldModel, sem
// checar nulo -- ver AbstractWeapon.cpp). Nao ha terreno, nao ha JSBSim, nao
// ha Station: so' os dois objetos construidos com 'new'.
//
#include "xnative/Paratrooper.hpp"
#include "xnative/ParatrooperAgentTC.hpp"

#include "mixr/models/WorldModel.hpp"
#include "mixr/simulation/AbstractPlayer.hpp"

#include "mixr/base/numeric/Integer.hpp"

#include <gtest/gtest.h>

#include <cmath>

namespace {

using namespace mixr;
using namespace mixr::models;
using namespace mixr::models::xparatrooper;

//------------------------------------------------------------------------------
// BenchWorld -- expoe 'setPhase()' (protegido em simulation::Simulation) so'
// para o teste poder simular as 4 fases do frame de tempo critico manualmente,
// sem levantar Station nenhuma.
//------------------------------------------------------------------------------
struct BenchWorld final : mixr::models::WorldModel
{
   using mixr::models::WorldModel::setPhase;
};

//------------------------------------------------------------------------------
// Bench -- um Paratrooper "de bancada", contido num WorldModel vazio (sem
// terreno). Sem terreno, 'tElev' fica em 0.0/invalido -- getAltitudeAglM()
// devolve a altitude MSL crua (documentado no header de ParatrooperBtBehavior
// como o motivo do slot 'requireTerrain'), o que da controle TOTAL sobre a
// AGL nos testes: setAltitude(x) e' o mesmo que "AGL = x" aqui.
//------------------------------------------------------------------------------
struct Bench
{
   BenchWorld* const world;
   Paratrooper* const trooper;

   Bench() : world(new BenchWorld()), trooper(new Paratrooper())
   {
      trooper->container(world);
      trooper->setID(9501);
      trooper->setInitMode(mixr::simulation::AbstractPlayer::ACTIVE);
      trooper->reset();
   }

   ~Bench()
   {
      trooper->unref();
      world->unref();
   }

   // Um frame completo (as 4 fases), dt = passo do frame INTEIRO (o mesmo
   // 'dt4' que Player::updateTC() calcula por dentro).
   void frame(const double dt)
   {
      const double dtPerPhase{dt / 4.0};
      for (unsigned int ph = 0; ph < 4; ++ph) {
         world->setPhase(ph);
         trooper->updateTC(dtPerPhase);
      }
   }
};

constexpr double kG{9.8066500286389};   // base::ETHG * base::distance::FT2M

//------------------------------------------------------------------------------
// Defaults do construtor.
//------------------------------------------------------------------------------
TEST(Paratrooper, NasceComTipoPARATROOPER)
{
   Bench bench;
   ASSERT_NE(bench.trooper->getType(), nullptr);
   EXPECT_STREQ("PARATROOPER", bench.trooper->getType()->getString());
}

TEST(Paratrooper, NasceInativoComoTodaArma)
{
   // AbstractWeapon::AbstractWeapon() forca INACTIVE -- e' o motivo de
   // 'mode: "ACTIVE"' ser OBRIGATORIO no .edl de qualquer cenario que
   // declare um Paratrooper direto em players: {} (ver
   // src/poc/paratrooper-drop). Um Effect/AbstractWeapon nasce assim para
   // nao "decolar sozinho" enquanto ainda esta pendurado numa estacao.
   Paratrooper trooper;
   EXPECT_TRUE(trooper.isInactive());
}

TEST(Paratrooper, MaxTofGenerosoEDragDeQuedaLivreHumana)
{
   Paratrooper trooper;
   EXPECT_DOUBLE_EQ(300.0, trooper.getMaxTOF());
   EXPECT_NEAR(0.18, trooper.getDragIndex(), 1e-9);

   const double vTerminal{kG / trooper.getDragIndex()};
   EXPECT_NEAR(54.48, vTerminal, 0.1);
}

TEST(Paratrooper, RaioLetalEAlcanceDeEstouroZerados)
{
   Paratrooper trooper;
   EXPECT_DOUBLE_EQ(0.0, trooper.getLethalRange());
   EXPECT_DOUBLE_EQ(0.0, trooper.getMaxBurstRng());
}

TEST(Paratrooper, MajorTypeEhWeapon)
{
   // Prova indireta de que NAO clampa ao terreno como um LIFE_FORM clamparia
   // (Player::positionUpdate() so' clampa GROUND_VEHICLE|SHIP|BUILDING|
   // LIFE_FORM) -- errado durante queda livre/velame.
   Paratrooper trooper;
   EXPECT_TRUE(trooper.isMajorType(Player::WEAPON));
}

//------------------------------------------------------------------------------
// Fisica por estagio.
//------------------------------------------------------------------------------
TEST(Paratrooper, FreefallAceleraParaBaixoESaturaNaTerminal)
{
   Bench bench;
   bench.trooper->setAltitude(3000.0);
   bench.trooper->setVelocity(0.0, 0.0, 0.0);

   double anterior{};
   for (int i = 0; i < 3000; ++i) {   // 3000 x 0,02s = 60s -- varios tau (g/k =~ 5,6s)
      bench.frame(0.02);
      const double atual{bench.trooper->getTotalVelocity()};
      EXPECT_GE(atual, anterior - 1e-6) << "iteracao " << i;   // monotonico (com folga numerica)
      anterior = atual;
   }

   const double vTerminal{kG / bench.trooper->getDragIndex()};
   EXPECT_NEAR(vTerminal, anterior, vTerminal * 0.01);
}

TEST(Paratrooper, CanopyImpoeTaxaConstanteSustentada)
{
   Bench bench;
   bench.trooper->setAltitude(3000.0);
   bench.trooper->setVelocity(10.0, 5.0, 40.0);   // "chegando" de uma queda livre
   bench.trooper->setJumpStage(domain::Stage::CANOPY);

   for (int i = 0; i < 100; ++i) {
      bench.frame(0.02);
      EXPECT_NEAR(bench.trooper->getCanopyDescentRateMps(), bench.trooper->getTotalVelocity(), 1e-6)
         << "iteracao " << i;
   }
}

TEST(Paratrooper, CanopyELandedZeramAtitudeDeRolamentoEArfagem)
{
   // Regressao contra o nariz-baixo que Effect::weaponDynamics() produziria
   // (atan2 da componente vertical contra velocidade de solo zero).
   Bench bench;
   bench.trooper->setAltitude(3000.0);
   bench.trooper->setJumpStage(domain::Stage::CANOPY);
   bench.frame(0.02);

   EXPECT_NEAR(0.0, bench.trooper->getRollR(), 1e-9);
   EXPECT_NEAR(0.0, bench.trooper->getPitchR(), 1e-9);

   bench.trooper->setJumpStage(domain::Stage::LANDED);
   bench.frame(0.02);

   EXPECT_NEAR(0.0, bench.trooper->getRollR(), 1e-9);
   EXPECT_NEAR(0.0, bench.trooper->getPitchR(), 1e-9);
}

TEST(Paratrooper, LandedZeraVelocidadeTotal)
{
   Bench bench;
   bench.trooper->setAltitude(0.5);
   bench.trooper->setVelocity(1.0, 1.0, 5.0);
   bench.trooper->setJumpStage(domain::Stage::LANDED);

   bench.frame(0.02);

   EXPECT_DOUBLE_EQ(0.0, bench.trooper->getTotalVelocity());
}

TEST(Paratrooper, LandedCongelaAPosicaoPermanentemente)
{
   Bench bench;
   bench.trooper->setAltitude(0.5);
   bench.trooper->setJumpStage(domain::Stage::LANDED);
   bench.frame(0.02);   // primeiro frame ja LANDED: velocidade zera aqui

   const base::Vec3d pos0{bench.trooper->getPosition()};

   for (int i = 0; i < 200; ++i) bench.frame(0.02);

   const base::Vec3d pos1{bench.trooper->getPosition()};
   EXPECT_DOUBLE_EQ(pos0[0], pos1[0]);
   EXPECT_DOUBLE_EQ(pos0[1], pos1[1]);
   EXPECT_DOUBLE_EQ(pos0[2], pos1[2]);
}

//------------------------------------------------------------------------------
// Tempo de voo (TOF) -- independente da AGL, puramente temporal.
//------------------------------------------------------------------------------
TEST(Paratrooper, TofAvancaEnquantoNoAr)
{
   Bench bench;
   bench.trooper->setAltitude(3000.0);

   for (int i = 0; i < 500; ++i) bench.frame(0.02);   // 10s -- Effect sozinho ja teria detonado em 10s

   EXPECT_TRUE(bench.trooper->isActive());
   EXPECT_FALSE(bench.trooper->isDetonated());
   EXPECT_GT(bench.trooper->getTOF(), 9.9);
}

TEST(Paratrooper, TofCongelaAoPousarENuncaAutodetonaParado)
{
   Bench bench;
   bench.trooper->setAltitude(0.5);
   bench.trooper->setJumpStage(domain::Stage::LANDED);
   bench.frame(0.02);

   const double tofAoPousar{bench.trooper->getTOF()};

   for (int i = 0; i < 20000; ++i) bench.frame(0.02);   // 400s -- muito alem de maxTOF (300s)

   EXPECT_DOUBLE_EQ(tofAoPousar, bench.trooper->getTOF());
   EXPECT_TRUE(bench.trooper->isActive());
   EXPECT_FALSE(bench.trooper->isDetonated());
}

TEST(Paratrooper, TofExpiraDeVerdadeSeNuncaPousar)
{
   // A via de escape continua funcionando: um Paratrooper que nunca chega a
   // LANDED (ex.: cenario mal configurado, deployAgl/groundAgl absurdos)
   // ainda respeita 'maxTOF' -- nao fica na simulacao para sempre.
   Bench bench;
   bench.trooper->setAltitude(1.0e9);   // nunca chega perto do chao

   // 'setMaxTOF()' e' protegida (AbstractWeapon.hpp) -- setSlotByName() e' o
   // mesmo caminho que um .edl usaria, so' que direto do teste.
   base::Integer cincoSegundos{5};
   bench.trooper->setSlotByName("maxTOF", &cincoSegundos);

   for (int i = 0; i < 400 && !bench.trooper->isDetonated(); ++i) bench.frame(0.02);   // ate 8s

   EXPECT_TRUE(bench.trooper->isDetonated());
}

//------------------------------------------------------------------------------
// A rede de seguranca contra o CRASH_EVENT generico -- o teste de maior
// valor desta suite: e' o que 'Effect' erraria sozinho (KILL_EVENT +
// dano/fumaca/chamas + DETONATED).
//------------------------------------------------------------------------------
TEST(Paratrooper, CrashGenericoViraLandedSemCascataDeDano)
{
   Bench bench;
   bench.trooper->setAltitude(-1000.0);   // forca AGL < 0 (sem terreno, AGL == MSL)
   bench.trooper->setVelocity(0.0, 0.0, 0.0);

   bench.frame(0.02);   // fase 0 desta frame roda dynamics() -> positionUpdate() -> CRASH_EVENT

   EXPECT_EQ(domain::Stage::LANDED, bench.trooper->getJumpStage());
   EXPECT_DOUBLE_EQ(0.0, bench.trooper->getDamage());
   EXPECT_DOUBLE_EQ(0.0, bench.trooper->getSmoke());
   EXPECT_DOUBLE_EQ(0.0, bench.trooper->getFlames());
   EXPECT_FALSE(bench.trooper->isDetonated());
   EXPECT_TRUE(bench.trooper->isActive());
}

TEST(Paratrooper, CrashOverrideNeutralizaTotalmenteOEventoDeCrash)
{
   // Controle negativo, parte 1: com 'crashOverride' ligado, nem sequer
   // transiciona para LANDED -- 'AbstractWeapon' respeita esta flag,
   // 'Effect' a ignora; Paratrooper::crashNotification() restaura o
   // contrato.
   Bench bench;
   bench.trooper->setCrashOverride(true);
   bench.trooper->setAltitude(-1000.0);
   bench.trooper->setVelocity(0.0, 0.0, 0.0);

   bench.frame(0.02);

   EXPECT_EQ(domain::Stage::FREEFALL, bench.trooper->getJumpStage());
   EXPECT_TRUE(bench.trooper->isActive());
}

TEST(Paratrooper, ColisaoGenericaTambemViraLandedSemDetonar)
{
   Bench bench;
   Paratrooper outro;

   bench.trooper->collisionNotification(&outro);

   EXPECT_EQ(domain::Stage::LANDED, bench.trooper->getJumpStage());
   EXPECT_FALSE(bench.trooper->isDetonated());
}

//------------------------------------------------------------------------------
// setJumpStage() -- idempotente (o log so' dispara na borda, mas o efeito
// observavel de repetir a mesma atribuicao tem que ser nulo).
//------------------------------------------------------------------------------
TEST(Paratrooper, SetJumpStageEhIdempotente)
{
   Paratrooper trooper;
   trooper.setJumpStage(domain::Stage::CANOPY);
   trooper.setJumpStage(domain::Stage::CANOPY);
   EXPECT_EQ(domain::Stage::CANOPY, trooper.getJumpStage());
}

//------------------------------------------------------------------------------
// O agente -- so' decide na fase 3, nunca no laco de fundo.
//------------------------------------------------------------------------------
TEST(ParatrooperAgentTC, SoDecideNaFase3)
{
   BenchWorld world;
   const auto trooper = new Paratrooper();
   trooper->container(&world);
   trooper->setID(9502);
   trooper->setInitMode(mixr::simulation::AbstractPlayer::ACTIVE);

   const auto agent = new ParatrooperAgentTC();
   agent->container(trooper);
   trooper->reset();

   for (unsigned int ph = 0; ph < 3; ++ph) {
      world.setPhase(ph);
      agent->updateTC(0.005);
   }
   EXPECT_EQ(0, agent->getDecisionCount());

   world.setPhase(3);
   agent->updateTC(0.005);
   EXPECT_EQ(1, agent->getDecisionCount());

   // O caminho de fundo (updateData()) e' NO-OP de proposito -- senao a FSM
   // avancaria mais de uma vez por frame.
   agent->updateData(0.1);
   EXPECT_EQ(1, agent->getDecisionCount());

   agent->unref();
   trooper->unref();
}

} // namespace
