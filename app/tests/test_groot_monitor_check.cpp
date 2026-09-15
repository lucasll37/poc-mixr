//
// app/GrootMonitorCheck.hpp -- confere se MIXR_GROOT_MONITOR nomeia um
// player que de fato existe no cenario, e loga o diagnostico certo quando
// nao (o header documenta que essa falha era 100% SILENCIOSA antes disto
// existir -- ver o cabecalho da classe. Alto valor de regressao, zero
// teste ate agora).
//
// A afirmacao e sobre o BUFFER EM MEMORIA de libs/xlog (mesma tecnica de
// app/tests/test_log_panel.cpp): ele e estado GLOBAL do processo, entao
// cada teste ancora no lastSeq() ANTES de chamar a funcao sob teste, e so
// olha as entradas que vieram DEPOIS -- rodar em qualquer ordem, ou junto
// de outras suites que tambem logam, tem que dar o mesmo resultado.
//
#include "app/GrootMonitorCheck.hpp"

#include "app/Fleet.hpp"

#include "xlog/Log.hpp"

#include "mixr/models/WorldModel.hpp"
#include "mixr/models/player/Player.hpp"

#include "mixr/base/Pair.hpp"
#include "mixr/base/PairStream.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <string>
#include <vector>

using mixr::xlog::Level;

namespace {

using namespace mixr;

// Mesmo racional de app/tests/test_log_panel.cpp: console desligado para
// nao sujar a saida do gtest, buffer intocado (e' o que se quer medir).
class GrootMonitorCheckTest : public ::testing::Test {
protected:
   void SetUp() override { xlog::setConsoleEnabled(false); }
   void TearDown() override { xlog::setLoggingEnabled(true); }
};

// RAII pra MIXR_GROOT_MONITOR -- restaura o valor anterior (ou a ausencia
// dele) ao sair, mesmo que o teste falhe no meio.
class EnvVarGuard {
public:
   EnvVarGuard(const char* const name, const char* const value) : name_(name)
   {
      const char* const prev{std::getenv(name)};
      hadPrev_ = (prev != nullptr);
      if (hadPrev_) prevValue_ = prev;

      if (value != nullptr) ::setenv(name, value, 1);
      else ::unsetenv(name);
   }

   ~EnvVarGuard()
   {
      if (hadPrev_) ::setenv(name_.c_str(), prevValue_.c_str(), 1);
      else ::unsetenv(name_.c_str());
   }

   EnvVarGuard(const EnvVarGuard&) = delete;
   EnvVarGuard& operator=(const EnvVarGuard&) = delete;

private:
   std::string name_;
   bool hadPrev_{};
   std::string prevValue_;
};

// WorldModel com players NOMEADOS -- mesmo mecanismo de test_fleet.cpp
// (setSlotByName("players", ...), o unico caminho publico: setSlotPlayers()
// e' um "slot table helper method" privado).
struct WorldBench
{
   models::WorldModel* const world{new models::WorldModel()};

   explicit WorldBench(const std::vector<std::string>& names)
   {
      const auto pairs = new base::PairStream();
      for (const std::string& name : names) {
         const auto player = new models::Player();
         const auto pair = new base::Pair(name.c_str(), player);
         player->unref();   // Pair::Pair() ja deu ref() -- devolve a nossa
         pairs->put(pair);
         pair->unref();      // List::addTail() ja deu ref()
      }
      world->setSlotByName("players", pairs);
      pairs->unref();
   }

   ~WorldBench() { world->unref(); }
};

// Todas as entradas do buffer com seq > 'desde' -- o que uma chamada
// especifica acrescentou, sem depender do estado de execucoes anteriores.
std::vector<xlog::Entry> logadoDesde(const std::uint64_t desde)
{
   std::vector<xlog::Entry> novas;
   for (auto& e : xlog::snapshot()) {
      if (e.seq > desde) novas.push_back(e);
   }
   return novas;
}

} // namespace

TEST_F(GrootMonitorCheckTest, SemVariavelDeAmbienteNaoLogaNada)
{
   EnvVarGuard env("MIXR_GROOT_MONITOR", nullptr);
   WorldBench bench({"falcon1", "falcon2"});

   const std::uint64_t antes{xlog::lastSeq()};
   app::checkGrootMonitorTarget(bench.world);

   EXPECT_TRUE(logadoDesde(antes).empty()) << "custo zero no caminho normal, sem variavel definida";
}

TEST_F(GrootMonitorCheckTest, WorldModelNuloNaoCrashaENaoLoga)
{
   EnvVarGuard env("MIXR_GROOT_MONITOR", "falcon1");

   const std::uint64_t antes{xlog::lastSeq()};
   EXPECT_NO_FATAL_FAILURE(app::checkGrootMonitorTarget(nullptr));
   EXPECT_TRUE(logadoDesde(antes).empty());
}

TEST_F(GrootMonitorCheckTest, PlayerNomeadoExisteLogaInfoComOProprioNome)
{
   EnvVarGuard env("MIXR_GROOT_MONITOR", "falcon1");
   WorldBench bench({"falcon1", "falcon2"});

   const std::uint64_t antes{xlog::lastSeq()};
   app::checkGrootMonitorTarget(bench.world);

   const auto novas{logadoDesde(antes)};
   ASSERT_EQ(novas.size(), 1u);
   EXPECT_EQ(novas[0].level, Level::INFO);
   EXPECT_NE(novas[0].text.find("falcon1"), std::string::npos);
   EXPECT_NE(novas[0].text.find("encontrado"), std::string::npos);
}

TEST_F(GrootMonitorCheckTest, PlayerNomeadoNaoExisteLogaWarningComALista)
{
   EnvVarGuard env("MIXR_GROOT_MONITOR", "fantasma");
   WorldBench bench({"falcon1", "falcon2"});

   const std::uint64_t antes{xlog::lastSeq()};
   app::checkGrootMonitorTarget(bench.world);

   const auto novas{logadoDesde(antes)};
   ASSERT_EQ(novas.size(), 1u);
   EXPECT_EQ(novas[0].level, Level::WARNING);
   EXPECT_NE(novas[0].text.find("fantasma"), std::string::npos);
   EXPECT_NE(novas[0].text.find("falcon1"), std::string::npos)
      << "a lista de players do cenario tem que aparecer no aviso, pra quem errou o nome saber o que existe";
   EXPECT_NE(novas[0].text.find("falcon2"), std::string::npos);
}
