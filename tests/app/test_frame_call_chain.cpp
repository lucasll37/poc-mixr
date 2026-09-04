// app/FrameCallChain.{hpp,cpp} -- a cadeia de chamadas do frame MIXR que a
// aba "Componentes" (F6) mostra: nome de funcao, argumento com o valor VIVO
// de dt, e a janela de rolagem que mantem a linha da fase corrente visivel.
//
// Sem MIXR e sem FTXUI (ComponentTreeQuery.hpp so forward-declara
// mixr::simulation::Station), no mesmo espirito de
// tests/app/test_component_flow_state.cpp.
//
// O que estes testes travam nao e "o texto esta bonito": e que os NUMEROS
// que aparecem no painel sao os que de fato circulam no framework -- em
// especial o dt que desce dividido por 4 (Player::tcFrame) e volta
// multiplicado por 4 (dt4, dentro de Player::updateTC). Errar isso seria
// ensinar errado, que e pior do que nao mostrar nada.
#include "app/FrameCallChain.hpp"

#include <gtest/gtest.h>

#include <string>

using app::CallChainLine;
using app::EstimatedPhase;
using app::FrameCallParams;
using app::buildFrameCallChain;
using app::frameDescentPath;
using app::nodeCallLabel;
using app::frameStepSeconds;
using app::isTimeCriticalPhase;

namespace {

FrameCallParams params50Hz(const bool paused = false)
{
   FrameCallParams p;
   p.tcRateHz = 50.0;
   p.bgRateHz = 10.0;
   p.fastForwardRate = 1;
   p.numTcThreads = 4;
   p.paused = paused;
   return p;
}

bool hasActiveContaining(const std::vector<CallChainLine>& chain, const std::string& needle)
{
   for (const auto& l : chain) {
      if (!l.active) continue;
      if (l.text.find(needle) != std::string::npos) return true;
      if (l.args.find(needle) != std::string::npos) return true;
   }
   return false;
}

bool hasAnyContaining(const std::vector<CallChainLine>& chain, const std::string& needle)
{
   for (const auto& l : chain) {
      if (l.text.find(needle) != std::string::npos) return true;
      if (l.args.find(needle) != std::string::npos) return true;
      if (l.note.find(needle) != std::string::npos) return true;
   }
   return false;
}

}   // namespace

TEST(FrameCallChain, DtSaiDaTaxaDaStation)
{
   EXPECT_DOUBLE_EQ(frameStepSeconds(params50Hz()), 0.02);

   FrameCallParams zero;
   zero.tcRateHz = 0.0;
   // Taxa invalida devolve ZERO, nao infinito: este valor vai direto para
   // station->tcFrame() no passo manual.
   EXPECT_DOUBLE_EQ(frameStepSeconds(zero), 0.0);
}

TEST(FrameCallChain, ODtDesceDivididoPorQuatroEVoltaMultiplicado)
{
   const auto chain{buildFrameCallChain(EstimatedPhase::DynamicsPhase0, params50Hz())};

   // Player::tcFrame recebe dt/4...
   EXPECT_TRUE(hasAnyContaining(chain, "0.005000"));
   // ...e dt4 devolve o dt do frame inteiro para quem roda 1x a cada 4 fases.
   EXPECT_TRUE(hasAnyContaining(chain, "0.020000"));
   EXPECT_TRUE(hasAnyContaining(chain, "dt4 = dt * 4"));
}

TEST(FrameCallChain, CadaFaseDestacaAChamadaQueElaDeFatoExecuta)
{
   const FrameCallParams p{params50Hz()};

   EXPECT_TRUE(hasActiveContaining(buildFrameCallChain(EstimatedPhase::DynamicsPhase0, p), "dynamics"));
   EXPECT_TRUE(hasActiveContaining(buildFrameCallChain(EstimatedPhase::DecisionPhase3, p), "process"));

   // A fase 1/2 do ciclo cobre DUAS fases do frame -- as duas tem de ficar
   // destacadas, senao a faixa mentiria sobre o que roda ali.
   const auto sensors{buildFrameCallChain(EstimatedPhase::SensorPhase1And2, p)};
   EXPECT_TRUE(hasActiveContaining(sensors, "transmit"));
   EXPECT_TRUE(hasActiveContaining(sensors, "receive"));
   EXPECT_FALSE(hasActiveContaining(sensors, "process"));
}

TEST(FrameCallChain, FasesDeFundoUsamAOutraCadeia)
{
   const FrameCallParams p{params50Hz()};

   EXPECT_TRUE(isTimeCriticalPhase(EstimatedPhase::Structural));
   EXPECT_TRUE(isTimeCriticalPhase(EstimatedPhase::DecisionPhase3));
   EXPECT_FALSE(isTimeCriticalPhase(EstimatedPhase::Background));
   EXPECT_FALSE(isTimeCriticalPhase(EstimatedPhase::DecisionBackground));

   const auto bg{buildFrameCallChain(EstimatedPhase::Background, p)};
   EXPECT_TRUE(hasAnyContaining(bg, "Station::updateData"));
   EXPECT_FALSE(hasAnyContaining(bg, "processTimeCriticalTasks"));

   // O ( SimAgent ) da poc single-thread decide no caminho de FUNDO -- e o
   // que separa esta fase da de tempo critico.
   EXPECT_TRUE(hasActiveContaining(buildFrameCallChain(EstimatedPhase::DecisionBackground, p),
                                   "Agent::controller"));
}

TEST(FrameCallChain, PausadoZeraODtQueDesce)
{
   const auto rodando{buildFrameCallChain(EstimatedPhase::DynamicsPhase0, params50Hz(false))};
   const auto pausado{buildFrameCallChain(EstimatedPhase::DynamicsPhase0, params50Hz(true))};

   EXPECT_TRUE(hasAnyContaining(rodando, "dt0 = isFrozen() ? 0.0 : dt"));
   EXPECT_TRUE(hasAnyContaining(pausado, "0.000000"));
   EXPECT_TRUE(hasAnyContaining(pausado, "PAUSADO"));
}

TEST(FrameCallChain, FastForwardRateApareceNoLaco)
{
   FrameCallParams p{params50Hz()};
   p.fastForwardRate = 64;
   EXPECT_TRUE(hasAnyContaining(buildFrameCallChain(EstimatedPhase::Structural, p),
                                "fastForwardRate = 64"));
}

//------------------------------------------------------------------------------
// O CAMINHO DA DESCIDA -- o que o card de detalhe mostra em "nesta fase".
//------------------------------------------------------------------------------
TEST(FrameDescentPath, MantemAAtivaEOsAncestraisEDescartaOsIrmaos)
{
   const auto chain{buildFrameCallChain(EstimatedPhase::DecisionPhase3, params50Hz())};
   const auto path{frameDescentPath(chain)};

   ASSERT_FALSE(path.empty());
   EXPECT_LT(path.size(), chain.size()) << "o caminho tem de ser um SUBCONJUNTO da cadeia";

   // A chamada da fase esta la...
   EXPECT_TRUE(hasActiveContaining(path, "process"));
   // ...e os ancestrais que levam ate ela tambem.
   EXPECT_TRUE(hasAnyContaining(path, "Simulation::updateTcPlayerList"));
   EXPECT_TRUE(hasAnyContaining(path, "Component::updateTC"));
   // Mas os IRMAOS que esta fase nao toma, nao.
   EXPECT_FALSE(hasAnyContaining(path, "transmit"));
   EXPECT_FALSE(hasAnyContaining(path, "receive"));
}

TEST(FrameDescentPath, ProfundidadeNuncaRetrocedeMaisDeUmNivelDeCadaVez)
{
   // Um caminho de descida tem de ser uma cadeia coerente: cada linha e
   // filha (ou irma) da anterior, nunca um salto para uma profundidade que
   // nao foi visitada.
   const auto path{frameDescentPath(buildFrameCallChain(EstimatedPhase::DynamicsPhase0, params50Hz()))};
   ASSERT_FALSE(path.empty());
   for (std::size_t i = 1; i < path.size(); i++) {
      EXPECT_LE(path[i].depth, path[i - 1].depth + 1);
   }
}

TEST(FrameDescentPath, DuasChamadasIrmasAtivasSobrevivemJuntas)
{
   // As fases 1 e 2 do ciclo marcam DUAS chamadas irmas -- o caminho tem de
   // manter as duas, senao o card mentiria sobre o que roda ali.
   const auto path{frameDescentPath(buildFrameCallChain(EstimatedPhase::SensorPhase1And2, params50Hz()))};
   EXPECT_TRUE(hasActiveContaining(path, "transmit"));
   EXPECT_TRUE(hasActiveContaining(path, "receive"));
}

//------------------------------------------------------------------------------
// O rotulo curto desenhado NO PROPRIO NO do canvas.
//------------------------------------------------------------------------------
TEST(NodeCallLabel, SoQuemParticipaDaFaseCorrenteGanhaRotulo)
{
   const FrameCallParams p{params50Hz()};

   EXPECT_EQ(nodeCallLabel(EstimatedPhase::DynamicsPhase0, EstimatedPhase::DynamicsPhase0, p),
             "dynamics(0.020s)");
   EXPECT_EQ(nodeCallLabel(EstimatedPhase::DecisionPhase3, EstimatedPhase::DecisionPhase3, p),
             "process(0.020s)");

   // Fase diferente -> nada desenhado. E o caso da esmagadora maioria dos
   // nos em qualquer instante; sem isso o canvas viraria uma sopa de texto.
   EXPECT_TRUE(nodeCallLabel(EstimatedPhase::DynamicsPhase0, EstimatedPhase::DecisionPhase3, p).empty());
   EXPECT_TRUE(nodeCallLabel(EstimatedPhase::Unknown, EstimatedPhase::Unknown, p).empty());
}

TEST(NodeCallLabel, ArgumentoDeFundoUsaODtDeFundo)
{
   FrameCallParams p{params50Hz()};
   p.bgRateHz = 10.0;
   // 1/10 Hz = 0.100 s -- e o dt do laco de fundo, nao o do frame T/C.
   EXPECT_EQ(nodeCallLabel(EstimatedPhase::Background, EstimatedPhase::Background, p),
             "updateData(0.100s)");
   EXPECT_EQ(nodeCallLabel(EstimatedPhase::DecisionBackground, EstimatedPhase::DecisionBackground, p),
             "controller(0.100s)");
}
