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
#include "mixr/models/player/air/Aircraft.hpp"
#include "mixr/simulation/AbstractPlayer.hpp"

#include "mixr/base/numeric/Integer.hpp"
#include "mixr/base/units/Distances.hpp"
#include "mixr/base/units/angle_utils.hpp"

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
// O PONTO DE SAIDA -- onde o paraquedista nasce em relacao a aeronave que o
// lancou.
//
// 'DropBench' reproduz o estado exato em que 'AbstractWeapon::release()' deixa
// o clone recem-criado -- container no WorldModel, aeronave lancadora
// apontada, modo PRE_RELEASE -- sem precisar de StoresMgr, estacao, Station
// ou cenario nenhum. UM frame basta: 'AbstractWeapon::updateTC()' promove
// PRE_RELEASE -> ACTIVE no FIM da fase 0, ou seja, logo depois de
// 'dynamics()' ja ter posicionado o paraquedista.
//
// Convencao de eixos, a mesma do MIXR: posicao e' NED
// (getPosition()[0]=norte, [1]=leste, [2]=BAIXO -- altitude e' -[2]).
//------------------------------------------------------------------------------
struct DropBench
{
   BenchWorld* const world;
   Aircraft* const carrier;
   Paratrooper* const trooper;

   DropBench(const double headingDeg, const double northM, const double eastM, const double altM)
      : world(new BenchWorld()), carrier(new Aircraft()), trooper(new Paratrooper())
   {
      carrier->container(world);
      carrier->setID(9001);
      carrier->setEulerAngles(0.0, 0.0, headingDeg * static_cast<double>(base::angle::D2RCC));
      carrier->setPosition(northM, eastM, -altM);

      trooper->container(world);
      trooper->setID(9601);
      trooper->setLaunchVehicle(carrier);
      trooper->setMode(Player::PRE_RELEASE);
   }

   ~DropBench()
   {
      trooper->unref();
      carrier->unref();
      world->unref();
   }

   // So a fase 0 -- e' a unica que chama dynamics(), e no fim dela o
   // paraquedista ja saiu de PRE_RELEASE.
   void releaseFrame()
   {
      world->setPhase(0);
      trooper->updateTC(0.005);
   }
};

TEST(Paratrooper, OffsetDeSaidaNasceEm15mAtrasE10mAbaixo)
{
   Paratrooper trooper;
   EXPECT_DOUBLE_EQ(15.0, trooper.getReleaseOffsetAftM());
   EXPECT_DOUBLE_EQ(10.0, trooper.getReleaseOffsetBelowM());
}

TEST(Paratrooper, NasceAtrasEAbaixoDaAeronaveQueLancou)
{
   // Aeronave rumo LESTE (90 deg) a 1500 m, sobre a origem do terreno de jogo.
   // "15 m atras" com o nariz para leste e' 15 m para OESTE (leste = -15).
   DropBench bench(90.0, 0.0, 0.0, 1500.0);
   bench.releaseFrame();

   const base::Vec3d pos{bench.trooper->getPosition()};
   EXPECT_NEAR(  0.0, pos[0], 1e-6);    // norte -- inalterado
   EXPECT_NEAR(-15.0, pos[1], 1e-6);    // leste -- 15 m atras do nariz
   EXPECT_NEAR(1490.0, -pos[2], 1e-6);  // altitude -- 10 m abaixo da aeronave

   // E ja esta valendo como jogador de verdade, nao mais preso ao lancador.
   EXPECT_TRUE(bench.trooper->isActive());
}

TEST(Paratrooper, OffsetDeSaidaAcompanhaORumoDaAeronave)
{
   // A prova de que o offset e' em eixos do CORPO, e nao um deslocamento fixo
   // em norte/leste: a MESMA aeronave, dois rumos, dois pontos de saida
   // diferentes -- sempre 15 m para TRAS do nariz dela.
   {
      DropBench norte(0.0, 4000.0, 7000.0, 1200.0);
      norte.releaseFrame();
      const base::Vec3d pos{norte.trooper->getPosition()};
      EXPECT_NEAR(3985.0, pos[0], 1e-6);   // 15 m ao SUL de uma aeronave rumo norte
      EXPECT_NEAR(7000.0, pos[1], 1e-6);
      EXPECT_NEAR(1190.0, -pos[2], 1e-6);
   }
   {
      DropBench oeste(270.0, 4000.0, 7000.0, 1200.0);
      oeste.releaseFrame();
      const base::Vec3d pos{oeste.trooper->getPosition()};
      EXPECT_NEAR(4000.0, pos[0], 1e-6);
      EXPECT_NEAR(7015.0, pos[1], 1e-6);   // 15 m a LESTE de uma aeronave rumo oeste
      EXPECT_NEAR(1190.0, -pos[2], 1e-6);
   }
}

TEST(Paratrooper, NasceComAVelocidadeDaAeronaveQueLancou)
{
   // Herdado do ramo nativo de AbstractWeapon::dynamics() -- quem sai pela
   // porta sai com a velocidade do aviao, nao parado no ar.
   DropBench bench(90.0, 0.0, 0.0, 1500.0);
   bench.carrier->setVelocity(0.0, 72.0, 0.0);   // 72 m/s para leste
   bench.releaseFrame();

   const base::Vec3d vel{bench.trooper->getVelocity()};
   EXPECT_NEAR( 0.0, vel[0], 1e-6);
   EXPECT_NEAR(72.0, vel[1], 1e-6);
}

TEST(Paratrooper, OffsetDeSaidaEhConfiguravelPorSlot)
{
   DropBench bench(90.0, 0.0, 0.0, 1500.0);

   base::Meters atras(40.0);
   base::Meters abaixo(25.0);
   ASSERT_TRUE(bench.trooper->setSlotByName("releaseOffsetAft", &atras));
   ASSERT_TRUE(bench.trooper->setSlotByName("releaseOffsetBelow", &abaixo));
   EXPECT_DOUBLE_EQ(40.0, bench.trooper->getReleaseOffsetAftM());
   EXPECT_DOUBLE_EQ(25.0, bench.trooper->getReleaseOffsetBelowM());

   bench.releaseFrame();

   const base::Vec3d pos{bench.trooper->getPosition()};
   EXPECT_NEAR(-40.0, pos[1], 1e-6);
   EXPECT_NEAR(1475.0, -pos[2], 1e-6);
}

TEST(Paratrooper, OffsetDeSaidaSobreviveAoCloneDaLiberacao)
{
   // 'AbstractWeapon::release()' nao lanca o objeto declarado no 'stores:' --
   // lanca um clone() dele. Sem o par em copyData(), o clone nasceria com o
   // default e um cenario que ajustou os slots seria ignorado em silencio.
   Paratrooper original;
   base::Meters atras(40.0);
   base::Meters abaixo(25.0);
   ASSERT_TRUE(original.setSlotByName("releaseOffsetAft", &atras));
   ASSERT_TRUE(original.setSlotByName("releaseOffsetBelow", &abaixo));

   const auto copia = original.clone();
   ASSERT_NE(copia, nullptr);
   EXPECT_DOUBLE_EQ(40.0, copia->getReleaseOffsetAftM());
   EXPECT_DOUBLE_EQ(25.0, copia->getReleaseOffsetBelowM());
   copia->unref();
}

TEST(Paratrooper, OffsetDeSaidaNaoTocaUmParaquedistaDeclaradoEmPlayers)
{
   // O caso de src/poc/paratrooper-drop: sem aeronave lancadora, os
   // 'initXPos'/'initYPos'/'initAlt' do .edl sao posicao ABSOLUTA no terreno
   // de jogo -- e tem de continuar sendo. O offset so' existe em PRE_RELEASE.
   Bench bench;
   bench.trooper->setInitPosition(1000.0, 2000.0);
   bench.trooper->setInitAltitude(3000.0);
   bench.trooper->reset();

   bench.frame(0.02);

   EXPECT_DOUBLE_EQ(1000.0, bench.trooper->getInitPosition()[0]);
   EXPECT_DOUBLE_EQ(2000.0, bench.trooper->getInitPosition()[1]);
   EXPECT_DOUBLE_EQ(3000.0, bench.trooper->getInitAltitude());
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
