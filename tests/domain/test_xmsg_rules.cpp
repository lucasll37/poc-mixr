// As primitivas de deteccao do shared/xmsg -- as quatro coisas que o MIXR nao
// tem e que um canal de eventos configuravel precisa ter.
//
// Este arquivo nao inclui nada do MIXR nem do BehaviorTree.CPP, de proposito:
// e o mesmo movimento que o repositorio ja fez uma vez, tirando o Snapshot de
// dentro de uma classe do framework para poder testar a regra sozinha.
//
// O que se trava aqui e comportamento de BORDA, que e onde este tipo de codigo
// erra: repetir no plato, oscilar na fronteira, disparar no transiente,
// perder o evento sob saturacao.

#include "xmsg/rules/Deadband.hpp"
#include "xmsg/rules/EmitGate.hpp"
#include "xmsg/rules/RateWindow.hpp"
#include "xmsg/rules/Schmitt.hpp"

#include <gtest/gtest.h>

namespace {

using mixr::xmsg::rules::Deadband;
using mixr::xmsg::rules::EmitGate;
using mixr::xmsg::rules::RateWindow;
using mixr::xmsg::rules::Schmitt;

constexpr double TOL{1e-9};

//------------------------------------------------------------------------------
// Schmitt
//------------------------------------------------------------------------------

Schmitt acima(const double trip, const double clear, const double hold = 0.0)
{
   Schmitt s;
   s.configure(Schmitt::Sense::Above, trip, clear, hold);
   return s;
}

TEST(Schmitt, HistereseDoLadoErradoEhRecusada)
{
   Schmitt s;
   // Above quer clear ABAIXO do trip; o inverso desarmaria junto com o armar
   // e o evento repetiria a cada ciclo na fronteira.
   EXPECT_FALSE(s.configure(Schmitt::Sense::Above, 0.5, 0.7, 0.0));
   EXPECT_TRUE(s.configure(Schmitt::Sense::Above, 0.5, 0.3, 0.0));

   EXPECT_FALSE(s.configure(Schmitt::Sense::Below, 500.0, 300.0, 0.0));
   EXPECT_TRUE(s.configure(Schmitt::Sense::Below, 500.0, 700.0, 0.0));
}

// Histerese degenerada e escolha, nao bug: a deteccao de borda sozinha ja
// impede a repeticao no plato. Quem exige 'clear:' e o MsgThreshold, no EDL.
TEST(Schmitt, HistereseDegeneradaEhAceitaEAindaNaoRepete)
{
   Schmitt s;
   ASSERT_TRUE(s.configure(Schmitt::Sense::Below, -10.0, -10.0, 0.0));

   EXPECT_FALSE(s.update(0.1, 0.0, true));
   EXPECT_TRUE(s.update(0.1, -25.0, true));
   for (int i = 0; i < 20; ++i) {
      EXPECT_FALSE(s.update(0.1, -25.0, true)) << "repetiu no plato";
   }
   EXPECT_FALSE(s.update(0.1, -5.0, true));
   EXPECT_FALSE(s.active());
   EXPECT_TRUE(s.update(0.1, -25.0, true)) << "nova travessia, nova borda";
}

TEST(Schmitt, EmiteSoNaBordaDeSubidaENaoRepeteNoPlato)
{
   auto s{acima(0.5, 0.3)};

   EXPECT_FALSE(s.update(0.1, 0.10, true));
   EXPECT_TRUE(s.update(0.1, 0.90, true)) << "a borda tinha de sair aqui";
   EXPECT_TRUE(s.active());

   // 50 ciclos bem acima do limiar: nenhuma mensagem nova
   for (int i = 0; i < 50; ++i) {
      EXPECT_FALSE(s.update(0.1, 0.95, true)) << "repetiu no plato, no ciclo " << i;
   }
   EXPECT_TRUE(s.active());
}

// O motivo de existir a histerese: sem ela, um valor tremendo em cima da
// fronteira produz uma mensagem por ciclo.
TEST(Schmitt, NaoOscilaNaFronteira)
{
   auto s{acima(0.5, 0.3)};

   ASSERT_TRUE(s.update(0.1, 0.60, true));

   // vai e volta em torno de 0.5, mas nunca abaixo de 0.3
   for (int i = 0; i < 20; ++i) {
      EXPECT_FALSE(s.update(0.1, 0.45, true));
      EXPECT_TRUE(s.active()) << "desarmou entre trip e clear, no ciclo " << i;
      EXPECT_FALSE(s.update(0.1, 0.55, true));
   }

   // so abaixo de clear desarma
   EXPECT_FALSE(s.update(0.1, 0.20, true));
   EXPECT_FALSE(s.active());

   // e ai uma nova travessia produz uma borda nova
   EXPECT_TRUE(s.update(0.1, 0.90, true));
}

TEST(Schmitt, HoldRejeitaTransienteCurto)
{
   auto s{acima(0.5, 0.3, 1.0)};   // exige 1 s continuo acima

   // 5 ciclos de 0.1 s acima do limiar = 0.5 s: nao arma
   for (int i = 0; i < 5; ++i) EXPECT_FALSE(s.update(0.1, 0.90, true));
   EXPECT_FALSE(s.active());

   // cai antes de cumprir o tempo: o relogio recomeca do zero
   EXPECT_FALSE(s.update(0.1, 0.10, true));

   for (int i = 0; i < 9; ++i) EXPECT_FALSE(s.update(0.1, 0.90, true));
   EXPECT_FALSE(s.active()) << "armou com menos de 1 s continuo";
   EXPECT_TRUE(s.update(0.1, 0.90, true)) << "1.0 s completo: agora arma";
}

TEST(Schmitt, SentidoBelowFunciona)
{
   Schmitt s;
   ASSERT_TRUE(s.configure(Schmitt::Sense::Below, 500.0, 700.0, 0.0));

   EXPECT_FALSE(s.update(0.1, 900.0, true));
   EXPECT_TRUE(s.update(0.1, 400.0, true));
   EXPECT_FALSE(s.update(0.1, 600.0, true)) << "entre trip e clear continua armado";
   EXPECT_TRUE(s.active());
   EXPECT_FALSE(s.update(0.1, 800.0, true));
   EXPECT_FALSE(s.active());
}

// A regra que impede o alarme falso no player que chega por DIS sem
// dynamicsModel: toda grandeza de motor dele le zero.
TEST(Schmitt, CampoInvalidoNaoAvaliaECongelaONivel)
{
   Schmitt s;
   ASSERT_TRUE(s.configure(Schmitt::Sense::Below, 500.0, 700.0, 0.0));

   // 0.0 casaria "abaixo de 500" com folga -- mas o campo nao existe
   for (int i = 0; i < 100; ++i) {
      EXPECT_FALSE(s.update(0.1, 0.0, false)) << "gerou borda em campo indisponivel";
      EXPECT_FALSE(s.active());
   }

   // com o campo valido, o nivel anterior tem de ser preservado
   ASSERT_TRUE(s.update(0.1, 400.0, true));
   EXPECT_TRUE(s.active());
   for (int i = 0; i < 10; ++i) {
      EXPECT_FALSE(s.update(0.1, 9999.0, false));
      EXPECT_TRUE(s.active()) << "campo invalido desarmou o que estava armado";
   }
}

TEST(Schmitt, HoldNaoEnvelheceComCampoInvalido)
{
   auto s{acima(0.5, 0.3, 1.0)};

   for (int i = 0; i < 5; ++i) EXPECT_FALSE(s.update(0.1, 0.90, true));   // 0.5 s
   for (int i = 0; i < 50; ++i) EXPECT_FALSE(s.update(0.1, 0.90, false)); // congelado
   for (int i = 0; i < 4; ++i) EXPECT_FALSE(s.update(0.1, 0.90, true));   // 0.9 s
   EXPECT_TRUE(s.update(0.1, 0.90, true)) << "1.0 s de tempo VALIDO completa o hold";
}

//------------------------------------------------------------------------------
// Deadband
//------------------------------------------------------------------------------

TEST(Deadband, PrimeiraAmostraValidaSempreEmite)
{
   Deadband d;
   d.configure(100.0);
   EXPECT_TRUE(d.update(1750.0, true));
   EXPECT_NEAR(d.reference(), 1750.0, TOL);
}

// A propriedade central: a comparacao e contra o ultimo valor EMITIDO, nao
// contra a amostra anterior. Uma subida lenta e continua tem de dar uma
// mensagem por degrau, nao uma por ciclo.
TEST(Deadband, MedeContraOUltimoEmitidoNaoContraOAnterior)
{
   Deadband d;
   d.configure(100.0);
   ASSERT_TRUE(d.update(1000.0, true));

   int emitidos{};
   // sobe 1 m por ciclo, 500 ciclos => 500 m de subida
   for (int i = 1; i <= 500; ++i) {
      if (d.update(1000.0 + i, true)) ++emitidos;
   }
   EXPECT_EQ(emitidos, 5) << "esperado um evento por degrau de 100 m";
}

TEST(Deadband, MudancaMenorQueOLimiarNaoEmite)
{
   Deadband d;
   d.configure(100.0);
   ASSERT_TRUE(d.update(1000.0, true));

   for (int i = 0; i < 50; ++i) {
      EXPECT_FALSE(d.update(1099.0, true));
      EXPECT_FALSE(d.update(901.0, true));
   }
   EXPECT_TRUE(d.update(1100.0, true)) << "exatamente no limiar deve emitir";
}

TEST(Deadband, ValeParaOsDoisSentidos)
{
   Deadband d;
   d.configure(50.0);
   ASSERT_TRUE(d.update(1000.0, true));
   EXPECT_TRUE(d.update(940.0, true)) << "descida tambem e mudanca";
   EXPECT_NEAR(d.reference(), 940.0, TOL);
}

TEST(Deadband, LimiarZeroEmiteAQualquerMudanca)
{
   Deadband d;
   d.configure(0.0);
   ASSERT_TRUE(d.update(2.0, true));
   EXPECT_FALSE(d.update(2.0, true)) << "valor igual nao e mudanca";
   EXPECT_TRUE(d.update(3.0, true));
   EXPECT_TRUE(d.update(2.0, true));
}

TEST(Deadband, CampoInvalidoNaoEmiteENaoMoveAReferencia)
{
   Deadband d;
   d.configure(100.0);
   ASSERT_TRUE(d.update(1000.0, true));

   for (int i = 0; i < 20; ++i) EXPECT_FALSE(d.update(5000.0, false));
   EXPECT_NEAR(d.reference(), 1000.0, TOL) << "amostra invalida moveu a referencia";
   EXPECT_TRUE(d.update(1150.0, true));
}

//------------------------------------------------------------------------------
// RateWindow
//------------------------------------------------------------------------------

TEST(RateWindow, PrecisaDeDoisPontosSeparadosNoTempo)
{
   RateWindow w;
   w.configure(1.0);
   EXPECT_FALSE(w.ready());
   w.push(0.1, 100.0, true);
   EXPECT_FALSE(w.ready());
   w.push(0.1, 110.0, true);
   EXPECT_TRUE(w.ready());
}

TEST(RateWindow, DerivadaConstanteSaiCerta)
{
   RateWindow w;
   w.configure(1.0);
   // 20 m/s de subida, amostrado a 10 Hz
   for (int i = 0; i <= 30; ++i) w.push(0.1, 1000.0 + 2.0 * i, true);
   ASSERT_TRUE(w.ready());
   EXPECT_NEAR(w.rate(), 20.0, 1e-6);
}

TEST(RateWindow, DescidaDaTaxaNegativa)
{
   RateWindow w;
   w.configure(1.0);
   for (int i = 0; i <= 30; ++i) w.push(0.1, 2000.0 - 1.5 * i, true);
   ASSERT_TRUE(w.ready());
   EXPECT_NEAR(w.rate(), -15.0, 1e-6);
}

// A janela e de tempo simulado: o que saiu dela nao pode influenciar a
// derivada de agora.
TEST(RateWindow, AmostraForaDaJanelaNaoContaMais)
{
   RateWindow w;
   w.configure(1.0);

   for (int i = 0; i < 30; ++i) w.push(0.1, 1000.0, true);   // 3 s parado
   ASSERT_TRUE(w.ready());
   EXPECT_NEAR(w.rate(), 0.0, 1e-6);

   for (int i = 1; i <= 20; ++i) w.push(0.1, 1000.0 + 5.0 * i, true);  // 2 s a 50 m/s
   EXPECT_NEAR(w.rate(), 50.0, 1e-6) << "o trecho parado, ja fora da janela, contaminou";
}

// A janela e a fonte unica de verdade para ready() E rate() -- os dois tem
// de concordar sobre "existe amostra suficiente DENTRO da janela?". Com uma
// janela mais estreita que o intervalo entre amostras (config incomum, mas
// nao rejeitada por configure()), a UNICA amostra anterior ja cai fora da
// janela: ready() tinha um bug em que respondia 'true' so por existir
// QUALQUER par com separacao > 0 no buffer inteiro, sem checar 'window_' --
// e rate() (que ja checava a janela corretamente) devolvia 0.0 por falta de
// separacao valida. Isso e exatamente o "derivada zero" que
// MsgRate::evaluate() existe para nao confundir com "sem derivada".
TEST(RateWindow, JanelaMenorQueOIntervaloDeAmostragemNaoFingeEstarPronta)
{
   RateWindow w;
   w.configure(0.01);   // janela de 10 ms

   w.push(0.1, 100.0, true);   // amostragem real a 10 Hz -- 100 ms entre pontos
   EXPECT_FALSE(w.ready());
   w.push(0.1, 200.0, true);
   EXPECT_FALSE(w.ready()) << "a unica amostra anterior esta a 100 ms, fora da janela de 10 ms";
   EXPECT_NEAR(w.rate(), 0.0, 1e-9);
}

// Contraprova: mesma configuracao, mas com um par de fato dentro da janela
// (amostragem mais rapida que a janela) -- ready() tem de virar true assim
// que ele aparece, nao ficar preso em false por causa do fix acima.
TEST(RateWindow, JanelaCurtaFicaProntaAssimQueHaAmostraDeVerdadeDentroDela)
{
   RateWindow w;
   w.configure(0.01);

   w.push(0.005, 100.0, true);
   EXPECT_FALSE(w.ready());
   w.push(0.005, 101.0, true);   // 5 ms depois -- dentro da janela de 10 ms
   ASSERT_TRUE(w.ready());
   EXPECT_NEAR(w.rate(), 200.0, 1e-6);   // 1 unidade em 5 ms
}

TEST(RateWindow, AmostraInvalidaNaoEntraMasOTempoCorre)
{
   RateWindow w;
   w.configure(1.0);
   w.push(0.1, 100.0, true);
   for (int i = 0; i < 5; ++i) w.push(0.1, 999.0, false);
   w.push(0.1, 106.0, true);

   ASSERT_TRUE(w.ready());
   // 6 unidades em 0.6 s de tempo simulado
   EXPECT_NEAR(w.rate(), 10.0, 1e-6);
}

//------------------------------------------------------------------------------
// EmitGate
//------------------------------------------------------------------------------

TEST(EmitGate, PrimeiraEmissaoSaiNaHora)
{
   EmitGate g;
   g.configure(5.0);
   EXPECT_TRUE(g.update(0.1, true)) << "o piso nao pode atrasar a primeira mensagem";
}

TEST(EmitGate, LimitaATaxaDeMensagemPeriodica)
{
   EmitGate g;
   g.configure(1.0);

   int emitidos{};
   // 10 s a 10 Hz, querendo emitir sempre
   for (int i = 0; i < 100; ++i) {
      if (g.update(0.1, true)) ++emitidos;
   }
   EXPECT_EQ(emitidos, 10) << "esperado ~1 por segundo";
}

// A propriedade que separa este desenho de um teto que descarta: a borda que
// cai dentro do piso nao se perde, ela espera.
TEST(EmitGate, BordaDentroDoPisoEhAdiadaNaoDescartada)
{
   EmitGate g;
   g.configure(5.0);

   ASSERT_TRUE(g.update(0.1, true));       // primeira sai
   EXPECT_FALSE(g.update(0.1, true));      // borda nova, dentro do piso
   EXPECT_EQ(g.deferred(), 1);
   EXPECT_TRUE(g.pending()) << "a borda tem de ficar pendurada";

   // nada mais quer sair, mas a pendente tem de sair quando o piso vencer
   bool saiu{false};
   for (int i = 0; i < 60 && !saiu; ++i) saiu = g.update(0.1, false);
   EXPECT_TRUE(saiu) << "a borda adiada nunca saiu -- foi descartada";
   EXPECT_FALSE(g.pending());
}

TEST(EmitGate, VariasBordasDentroDoPisoColapsamEmUma)
{
   EmitGate g;
   g.configure(5.0);
   ASSERT_TRUE(g.update(0.1, true));

   for (int i = 0; i < 40; ++i) g.update(0.1, true);   // 4 s de bordas
   EXPECT_EQ(g.deferred(), 40);

   int saidas{};
   for (int i = 0; i < 20; ++i) {
      if (g.update(0.1, false)) ++saidas;
   }
   EXPECT_EQ(saidas, 1) << "as bordas coalescem numa emissao, nao numa fila";
}

TEST(EmitGate, PisoZeroEmiteSempreQueHaAlgo)
{
   EmitGate g;
   g.configure(0.0);
   int emitidos{};
   for (int i = 0; i < 50; ++i) {
      if (g.update(0.02, true)) ++emitidos;
   }
   EXPECT_EQ(emitidos, 50);
   EXPECT_EQ(g.deferred(), 0);
}

TEST(EmitGate, SemNadaQuerendoSairNaoEmite)
{
   EmitGate g;
   g.configure(1.0);
   for (int i = 0; i < 100; ++i) {
      EXPECT_FALSE(g.update(0.1, false));
   }
   EXPECT_EQ(g.deferred(), 0);
}

} // namespace
