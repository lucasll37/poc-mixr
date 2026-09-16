//
// Camada 3 do MODELO -- as classes MIXR proprias, SEM levantar Station.
//
// 'BenchWorld' e' o UNICO WorldModel que estes testes levantam, e so' para
// dar a Navstar3AgentTC::controller() o 'getWorldModel()->phase()' de que
// ela depende. Nao ha terreno, nao ha Station: so' os objetos construidos
// com 'new'.
//
// O propagador orbital (domain::CircularOrbit/EclipseGeometry) ja tem sua
// PROPRIA suite, sem MIXR (tests/domain/) -- aqui a pergunta e outra: "o
// Player de verdade se move quando o UBF decide", nao "a formula esta
// certa".
//
#include "ubf/Navstar3Action.hpp"
#include "ubf/Navstar3BtBehavior.hpp"
#include "ubf/Navstar3State.hpp"
#include "xnative/Navstar3AgentTC.hpp"

#include "xboard/Board.hpp"

#include "mixr/models/WorldModel.hpp"
#include "mixr/models/player/Player.hpp"
#include "mixr/models/player/space/SpaceVehicle.hpp"
#include "mixr/simulation/AbstractPlayer.hpp"

#include "mixr/base/units/Angles.hpp"
#include "mixr/base/units/Distances.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <cmath>
#include <set>

namespace {

using namespace mixr;
using namespace mixr::models;
using namespace mixr::models::xNavstar_3;

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
// Bench -- um ( SpaceVehicle ) "de bancada" com o agente/percepcao/decisao
// deste modelo ja fiados, do MESMO jeito que o .edl faria (setSlotByName(),
// o mesmo caminho publico que o parser de EDL usa -- nao ha atalho C++
// privado sendo exercitado aqui). 'treeFile' fica vazio de proposito: o
// ramo de DEGRADACAO de Navstar3BtBehavior::genAction() decide pela MESMA
// geometria sol/sombra, sem precisar de um caminho de arquivo .xml valido
// dentro da bancada -- a arvore de verdade ja tem sua propria suite
// (tests/tree/).
//------------------------------------------------------------------------------
struct Bench
{
   BenchWorld* const world;
   SpaceVehicle* const sat;
   Navstar3AgentTC* const agent;

   explicit Bench(double altitudeM = 20180000.0, double inclinationDeg = 55.0,
                  double raanDeg = 0.0, double argLat0Deg = 0.0,
                  double sunRaDeg = 0.0, double sunDecDeg = 0.0)
      : world(new BenchWorld()), sat(new SpaceVehicle()), agent(new Navstar3AgentTC())
   {
      sat->container(world);
      // ID UNICO por instancia -- xboard::get()/setBehaviorLabel() sao
      // GLOBAIS ao processo, chaveados por ID (libs/xboard/Board.hpp), e o
      // gtest roda todos os TEST() do binario no MESMO processo. Um ID fixo
      // repetido entre bancadas faria o contador de decisoes de um teste
      // "vazar" para o proximo que reusasse o mesmo numero.
      static std::atomic<int> nextId{9601};
      sat->setID(nextId.fetch_add(1));
      sat->setInitMode(mixr::simulation::AbstractPlayer::ACTIVE);

      agent->container(sat);

      const auto state = new Navstar3State();
      const auto behavior = new Navstar3BtBehavior();

      base::Meters altitude{altitudeM};
      base::Degrees inclination{inclinationDeg};
      base::Degrees raan{raanDeg};
      base::Degrees argLat0{argLat0Deg};
      base::Degrees sunRa{sunRaDeg};
      base::Degrees sunDec{sunDecDeg};
      behavior->setSlotByName("altitude", &altitude);
      behavior->setSlotByName("inclination", &inclination);
      behavior->setSlotByName("raan", &raan);
      behavior->setSlotByName("argLat0", &argLat0);
      behavior->setSlotByName("sunRightAscension", &sunRa);
      behavior->setSlotByName("sunDeclination", &sunDec);

      agent->setSlotByName("state", state);
      agent->setSlotByName("behavior", behavior);
      state->unref();
      behavior->unref();

      sat->reset();
   }

   ~Bench()
   {
      agent->unref();
      sat->unref();
      world->unref();
   }

   // Um frame completo (as 4 fases). O agente e' acionado DIRETO (nao via
   // sat->updateTC(), que nao encadearia o agente sem um Component::
   // addComponent() de verdade) -- mesmo padrao ja usado pelo modelo
   // paratrooper deste repositorio.
   void frame(const double dt)
   {
      const double dtPerPhase{dt / 4.0};
      for (unsigned int ph = 0; ph < 4; ++ph) {
         world->setPhase(ph);
         agent->updateTC(dtPerPhase);
      }
   }
};

//------------------------------------------------------------------------------
// Percepcao.
//------------------------------------------------------------------------------
TEST(Navstar3State, SemAtorValidoNaoTemLeitura)
{
   Navstar3State state;
   EXPECT_FALSE(state.hasReading());
}

TEST(Navstar3State, ComPlayerValidoGuardaAAltitude)
{
   // setAltitude() so' funciona com 'useCoordSys' resolvido (CS_LOCAL,
   // CS_GEOD ou CS_WORLD) -- um Player recem-construido nasce em CS_NONE
   // (Player.hpp:950), e reset() e' quem resolve isso (Player.cpp:465-479).
   // Precisa de um WorldModel container: setPosition()/setAltitude() leem
   // 'getWorldModel()->getMaxRefRange()' sem checar nulo.
   BenchWorld world;
   SpaceVehicle sat;
   sat.container(&world);
   sat.reset();
   sat.setAltitude(20180000.0);

   Navstar3State state;
   state.updateState(&sat);

   EXPECT_TRUE(state.hasReading());
   EXPECT_NEAR(20180000.0, state.getAltitudeM(), 1.0);
}

//------------------------------------------------------------------------------
// A escolha de classe base -- prova indireta de que o ground-clamping
// nativo (Player::positionUpdate(), so' para GROUND_VEHICLE|SHIP|BUILDING|
// LIFE_FORM) nunca se aplica a este player, mesmo com terreno carregado no
// cenario.
//------------------------------------------------------------------------------
TEST(SpaceVehicle, MajorTypeEhSpaceVehicle)
{
   SpaceVehicle sat;
   EXPECT_TRUE(sat.isMajorType(Player::SPACE_VEHICLE));
}

//------------------------------------------------------------------------------
// O agente -- so' decide na fase 3, nunca no laco de fundo. Mesmo padrao ja
// medido para os agentes deste repositorio (A-4/C-130/paratrooper).
//------------------------------------------------------------------------------
TEST(Navstar3AgentTC, SoDecideNaFase3)
{
   Bench bench;

   for (unsigned int ph = 0; ph < 3; ++ph) {
      bench.world->setPhase(ph);
      bench.agent->updateTC(0.005);
   }
   EXPECT_EQ(0, bench.agent->getDecisionCount());

   bench.world->setPhase(3);
   bench.agent->updateTC(0.005);
   EXPECT_EQ(1, bench.agent->getDecisionCount());

   // O caminho de fundo (updateData()) e' NO-OP de proposito -- senao a
   // orbita avancaria mais de uma vez por frame.
   bench.agent->updateData(0.1);
   EXPECT_EQ(1, bench.agent->getDecisionCount());
}

//------------------------------------------------------------------------------
// A cadeia inteira, de ponta a ponta: percepcao -> decisao -> atuacao move
// o PLAYER de verdade, sem Station nenhuma. dt sintetico grande (100 s) --
// a granularidade fina (0,02 s a 50 Hz) e' responsabilidade do
// determinismo medido rodando o cenario de verdade
// (tests/determinism/check_determinism.sh), nao desta bancada.
//------------------------------------------------------------------------------
TEST(Navstar3AgentTC, MoveOPlayerDeVerdadeEMantemAAltitude)
{
   Bench bench;

   const double lat0{bench.sat->getLatitude()};
   const double lon0{bench.sat->getLongitude()};

   for (int i = 0; i < 5; ++i) bench.frame(100.0);

   const double lat1{bench.sat->getLatitude()};
   const double lon1{bench.sat->getLongitude()};

   EXPECT_TRUE(std::abs(lat1 - lat0) > 1e-6 || std::abs(lon1 - lon0) > 1e-6)
      << "posicao nao mudou entre frames -- o integrador nativo esta"
      << " competindo com o calculo orbital, ou setGeocPosition() nao rodou";

   // Round-trip de convertGeod2Ecef()/convertEcef2Geod() (a MESMA formula,
   // ida e volta): a altitude HAE lida de volta do Player bate com a que
   // foi configurada, a precisao de ponto flutuante.
   EXPECT_NEAR(20180000.0, bench.sat->getAltitudeM(), 1.0);
}

//------------------------------------------------------------------------------
// xboard -- a obrigacao que falha em SILENCIO se esquecida (CONTRATO.md
// secao 3). Depois de UM ciclo completo de decisao, o rotulo nao pode
// continuar no default "--" e o contador de decisoes tem que ter avancado.
//------------------------------------------------------------------------------
TEST(Navstar3Action, EscreveNoXboard)
{
   Bench bench;
   bench.frame(20.0);

   const xboard::Readout r{xboard::get(bench.sat->getID())};
   EXPECT_NE("--", r.label);
   EXPECT_EQ(1, r.decisions);
}

//------------------------------------------------------------------------------
// Eclipse -- ao longo de uma orbita SINTETICA completa (dt grande, so' para
// varrer o ciclo depressa), o rotulo tem que passar por ECLIPSE E por
// SUNLIT -- nao travar sempre no mesmo, o que indicaria que a geometria
// nunca esta sendo recalculada de verdade.
//------------------------------------------------------------------------------
TEST(Navstar3AgentTC, RotuloAlternaEntreSunlitEEclipseAoLongoDeUmaOrbita)
{
   // sunRightAscension=180 poe o vetor do sol em (-1,0,0) -- com os
   // elementos orbitais default (raan=0, argLat0=0), o satelite nasce
   // exatamente na sombra (t=0) e sai dela por volta de t=1700s, calculado
   // a mao contra a MESMA formula de domain::eciPosition()/sunState().
   Bench bench{/*altitudeM=*/20180000.0, /*inclinationDeg=*/55.0, /*raanDeg=*/0.0,
               /*argLat0Deg=*/0.0, /*sunRaDeg=*/180.0, /*sunDecDeg=*/0.0};

   std::set<std::string> rotulosVistos;
   for (int i = 0; i < 432; ++i) {   // ~1 orbita completa (periodo ~43073s / 100s por frame)
      bench.frame(100.0);
      rotulosVistos.insert(xboard::get(bench.sat->getID()).label);
   }

   EXPECT_EQ((std::set<std::string>{"ECLIPSE", "SUNLIT"}), rotulosVistos);
}

} // namespace
