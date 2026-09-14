// libs/xclock::ClockStation -- setTimeScale()/getTimeScale()/pausa, sem
// levantar simulacao nenhuma (mesmo espirito de test_xboard_concurrency.cpp:
// a classe e construivel e testavel direto, sem EDL/Station de verdade por
// tras). Cobre a "fonte unica de verdade" documentada no proprio .hpp --
// abaixo de 1x quem manda e o slowFactor privado, de 1x pra cima quem manda
// e o fastForwardRate NATIVO da Station -- e a degradacao graciosa sem
// getSimulation() (isPaused()/setPaused() nunca desreferenciam nulo).
#include "xclock/ClockStation.hpp"

#include <gtest/gtest.h>

namespace {

TEST(ClockStation, VelocidadeInicialEUmXComoOFastForwardRateNativoPadrao) {
   mixr::xclock::ClockStation cs;
   EXPECT_DOUBLE_EQ(cs.getTimeScale(), 1.0);
   EXPECT_EQ(cs.getFastForwardRate(), 1u);
}

TEST(ClockStation, AcelerarAcimaDeUmXVaiParaOFastForwardRateNativoArredondado) {
   mixr::xclock::ClockStation cs;
   EXPECT_TRUE(cs.setTimeScale(2.5));
   // setTimeScale() arredonda pro inteiro que o slot nativo aceita
   // (static_cast<unsigned int>(scale + 0.5)) -- 2.5 -> 3.
   EXPECT_EQ(cs.getFastForwardRate(), 3u);
   EXPECT_DOUBLE_EQ(cs.getTimeScale(), 3.0);
}

TEST(ClockStation, FrearAbaixoDeUmXUsaOSlowFactorPrivadoNaoOFastForwardRate) {
   mixr::xclock::ClockStation cs;
   EXPECT_TRUE(cs.setTimeScale(0.25));
   EXPECT_DOUBLE_EQ(cs.getTimeScale(), 0.25);
   // O framework nao tem como pedir "meio frame" -- por baixo o
   // fastForwardRate nativo fica em 1 (um frame por periodo real), e quem
   // encurta o dt e' processTimeCriticalTasks(), nunca exercitado aqui.
   EXPECT_EQ(cs.getFastForwardRate(), 1u);
}

TEST(ClockStation, VoltarDeCamaraLentaParaUmXZeraOSlowFactor) {
   mixr::xclock::ClockStation cs;
   ASSERT_TRUE(cs.setTimeScale(0.1));
   ASSERT_DOUBLE_EQ(cs.getTimeScale(), 0.1);
   EXPECT_TRUE(cs.setTimeScale(1.0));
   EXPECT_DOUBLE_EQ(cs.getTimeScale(), 1.0);
}

TEST(ClockStation, ForaDaFaixaDevolveFalseENaoMudaOEstadoAnterior) {
   mixr::xclock::ClockStation cs;
   ASSERT_TRUE(cs.setTimeScale(2.0));

   EXPECT_FALSE(cs.setTimeScale(mixr::xclock::ClockStation::getMinTimeScale() - 0.01));
   EXPECT_FALSE(cs.setTimeScale(mixr::xclock::ClockStation::getMaxTimeScale() + 1.0));
   // O pedido invalido nao deve ter mexido no valor ja armado.
   EXPECT_DOUBLE_EQ(cs.getTimeScale(), 2.0);
}

TEST(ClockStation, LimitesEstaticosBatemComOsDocumentadosNoCpp) {
   EXPECT_DOUBLE_EQ(mixr::xclock::ClockStation::getMinTimeScale(), 0.05);
   EXPECT_DOUBLE_EQ(mixr::xclock::ClockStation::getMaxTimeScale(), 1000.0);
}

TEST(ClockStation, PausaSemSimulacaoConfiguradaDegradaGraciosamenteSemDerrubar) {
   mixr::xclock::ClockStation cs;
   // Nenhum 'simulation:' foi declarado (nenhum EDL passou por aqui) --
   // getSimulation() e' nulo, e isPaused()/setPaused() tem de tratar isso
   // sem desreferenciar nada (ver o guard 'if (sim == nullptr) return false'
   // em ClockStation.cpp).
   EXPECT_FALSE(cs.isPaused());
   EXPECT_FALSE(cs.setPaused(true));
   EXPECT_FALSE(cs.isPaused());
   EXPECT_FALSE(cs.togglePaused());
}

TEST(ClockStation, TcIdleTicksComecaEmZero) {
   mixr::xclock::ClockStation cs;
   EXPECT_EQ(cs.tcIdleTicks(), 0u);
}

TEST(ClockStation, WaitForTcQuiescedSemThreadTcDevolveTrueNaHora) {
   mixr::xclock::ClockStation cs;
   // Sem 'createTimeCriticalProcess()' chamado (nenhuma Station de verdade
   // foi montada), doWeHaveTheTcThread() e' falso -- waitForTcQuiesced() nao
   // tem por que esperar nada, e o proprio .hpp documenta isso.
   EXPECT_TRUE(cs.waitForTcQuiesced(0.01));
}

TEST(ClockStation, RequestStepENaoDerrubaSemPausaAtiva) {
   mixr::xclock::ClockStation cs;
   // requestStep() e' seguro de chamar mesmo sem pausa/simulacao -- so'
   // acumula um contador interno, consumido dentro de
   // processTimeCriticalTasks() (nunca exercitado nesta suite, que nao
   // levanta thread de tempo critico nenhuma). O teste aqui e' so' "nao
   // trava, nao derruba".
   cs.requestStep();
   cs.requestStep(3);
   SUCCEED();
}

}  // namespace
