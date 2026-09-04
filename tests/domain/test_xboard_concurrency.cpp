#include "xboard/Board.hpp"

#include <gtest/gtest.h>

#include <thread>
#include <vector>

// shared/xboard e a UNICA shared_library() de shared/ (as outras cinco sao
// estaticas) -- precisamente porque e escrita pelo MODELO (num .so aberto
// com dlopen, ate N threads do pool T/C) e lida pelo HOST (thread de
// background). Ver o "porque" completo em xboard/Board.hpp. Esta e a
// UNICA responsabilidade do modulo -- "ser um mapa seguro sob N
// escritores concorrentes e 1 leitor" -- nunca tinha teste direto: so
// validacao indireta pelos numeros 'dec='/'bt='/'thread=' nos dumps
// end-to-end.

namespace {
constexpr int kThreads{8};
constexpr int kIncrementsPerThread{200000};
constexpr long kExpectedTotal{static_cast<long>(kThreads) * kIncrementsPerThread};
}

TEST(XBoardConcurrency, IncrementosConcorrentesSaoExatos)
{
   constexpr int playerId{7001};   // id proprio deste teste -- nao colide com outros
   std::vector<std::thread> workers;
   for (int t = 0; t < kThreads; t++) {
      workers.emplace_back([] {
         for (int i = 0; i < kIncrementsPerThread; i++) mixr::xboard::bumpDecisionCount(playerId);
      });
   }
   for (auto& w : workers) w.join();

   EXPECT_EQ(mixr::xboard::get(playerId).decisions, kExpectedTotal)
      << kThreads << " threads x " << kIncrementsPerThread
      << " incrementos concorrentes tem que somar EXATO, sem incremento perdido.";
}

TEST(XBoardConcurrency, EscritoresConcorrentesEmChavesDiferentesNaoInterferem)
{
   // Cada thread escreve SO no seu proprio playerId -- prova que insercoes
   // concorrentes em chaves DIFERENTES do std::map (que pode reequilibrar
   // a arvore rubro-negra internamente) nao se corrompem entre si sob o
   // mutex unico.
   constexpr int kBase{7100};
   constexpr int kNumPlayers{8};
   std::vector<std::thread> workers;
   for (int p = kBase; p < kBase + kNumPlayers; p++) {
      workers.emplace_back([p] {
         for (int i = 0; i < 1000; i++) {
            mixr::xboard::setThreadTag(p, p);   // valor previsivel: tag == id
            mixr::xboard::bumpDecisionCount(p);
         }
      });
   }
   for (auto& w : workers) w.join();

   for (int p = kBase; p < kBase + kNumPlayers; p++) {
      const auto r{mixr::xboard::get(p)};
      EXPECT_EQ(r.threadTag, p);
      EXPECT_EQ(r.decisions, 1000);
   }
}

TEST(XBoardConcurrency, GetEmPlayerIdDesconhecidoDevolveDefault)
{
   const auto r{mixr::xboard::get(-999999)};
   EXPECT_EQ(r.label, "--");
   EXPECT_EQ(r.decisions, 0);
   EXPECT_EQ(r.threadTag, -1);
}

// Os quatro setters abaixo (label/alert/datalink/radar) nunca tinham teste
// direto -- so bumpDecisionCount()/setThreadTag() tinham, na bateria acima.
// Correcao, nao so cobertura: sao exatamente os campos que
// FlightAction::execute() escreve a cada decisao (ver CLAUDE.md, secao
// "shared/xboard"), e cada um grava por conta propria sob o MESMO mutex --
// o que faltava provar e que nenhum deles pisa no que os outros ja
// escreveram no mesmo Readout.
TEST(XBoardConcurrency, SetBehaviorLabelGravaOTextoExato)
{
   constexpr int playerId{7201};
   mixr::xboard::setBehaviorLabel(playerId, "EVADE");
   EXPECT_EQ(mixr::xboard::get(playerId).label, "EVADE");

   mixr::xboard::setBehaviorLabel(playerId, "SUPPORT");
   EXPECT_EQ(mixr::xboard::get(playerId).label, "SUPPORT");
}

TEST(XBoardConcurrency, SetAlertGravaOsTresCamposJuntos)
{
   constexpr int playerId{7202};
   mixr::xboard::setAlert(playerId, true, "falcon1", "bandit1");

   const auto r{mixr::xboard::get(playerId)};
   EXPECT_TRUE(r.alertValid);
   EXPECT_EQ(r.alertSender, "falcon1");
   EXPECT_EQ(r.alertContact, "bandit1");

   // Desligar o alerta nao e so zerar 'alertValid' -- confirma que a
   // funcao sempre sobrescreve os tres campos juntos, nunca so um.
   mixr::xboard::setAlert(playerId, false, "", "");
   const auto r2{mixr::xboard::get(playerId)};
   EXPECT_FALSE(r2.alertValid);
   EXPECT_EQ(r2.alertSender, "");
   EXPECT_EQ(r2.alertContact, "");
}

TEST(XBoardConcurrency, SetDatalinkCountersGravaSentERecebidoSeparados)
{
   constexpr int playerId{7203};
   mixr::xboard::setDatalinkCounters(playerId, 3, 5);

   const auto r{mixr::xboard::get(playerId)};
   EXPECT_EQ(r.sent, 3);
   EXPECT_EQ(r.received, 5);
}

TEST(XBoardConcurrency, SetRadarScanGravaOsCincoCamposDaVarredura)
{
   constexpr int playerId{7204};
   mixr::xboard::setRadarScan(playerId, true, 45.0, -10.0, 12000.0, 3.0, 2.0);

   const auto r{mixr::xboard::get(playerId)};
   EXPECT_TRUE(r.radarValid);
   EXPECT_DOUBLE_EQ(r.radarAzDeg, 45.0);
   EXPECT_DOUBLE_EQ(r.radarElDeg, -10.0);
   EXPECT_DOUBLE_EQ(r.radarRangeM, 12000.0);
   EXPECT_DOUBLE_EQ(r.radarHBeamDeg, 3.0);
   EXPECT_DOUBLE_EQ(r.radarVBeamDeg, 2.0);
}

// Estresse combinado: N threads escrevendo TODOS os setters no MESMO
// player ao mesmo tempo (o padrao real de uso -- FlightAction::execute()
// roda os quatro em sequencia dentro de uma so decisao, e com
// FlightAgentTC ha uma thread do pool T/C por aviao). O que se prova aqui
// nao e atomicidade CRUZADA entre campos (o design nunca prometeu isso --
// cada setter toma o mutex por conta propria), e sim que a estrutura
// nao se corrompe: 'get()' apos todas as threads terminarem tem de
// devolver SEMPRE um dos valores escritos por alguma thread, nunca uma
// mistura de bytes de duas escritas (o que aconteceria se std::string, por
// exemplo, fosse lido/escrito fora do mutex).
TEST(XBoardConcurrency, EscritaConcorrenteDeTodosOsSettersNoMesmoPlayerNaoCorrompe)
{
   constexpr int playerId{7205};
   constexpr int kThreadsPerField{4};
   constexpr int kItersPerThread{5000};

   std::vector<std::thread> workers;
   for (int t = 0; t < kThreadsPerField; t++) {
      workers.emplace_back([t] {
         for (int i = 0; i < kItersPerThread; i++) {
            mixr::xboard::setBehaviorLabel(playerId, t % 2 == 0 ? "EVADE" : "SUPPORT");
            mixr::xboard::bumpDecisionCount(playerId);
            mixr::xboard::setThreadTag(playerId, t);
            mixr::xboard::setAlert(playerId, t % 2 == 0, "falconX", "bandit1");
            mixr::xboard::setDatalinkCounters(playerId, t, t);
            mixr::xboard::setRadarScan(playerId, true, t, t, t, t, t);
         }
      });
   }
   for (auto& w : workers) w.join();

   const auto r{mixr::xboard::get(playerId)};
   EXPECT_EQ(r.decisions, static_cast<long>(kThreadsPerField) * kItersPerThread);
   // O rotulo final tem de ser um dos dois valores possiveis -- nunca uma
   // string truncada/corrompida por uma escrita interrompida a meio.
   EXPECT_TRUE(r.label == "EVADE" || r.label == "SUPPORT");
   EXPECT_TRUE(r.threadTag >= 0 && r.threadTag < kThreadsPerField);
   EXPECT_EQ(r.alertSender, "falconX");
   EXPECT_EQ(r.alertContact, "bandit1");
}
