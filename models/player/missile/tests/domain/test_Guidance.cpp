#include "domain/Guidance.hpp"

#include <gtest/gtest.h>

#include <cmath>

// A lei de guiagem e pura (ver domain/Guidance.hpp) -- estes casos travam o
// comportamento observado, nao reimplementam a formula.

TEST(Guidance, AlvoNaProaComandoNeutro)
{
   // Alvo 1000 m a frente (NED: norte positivo), mesmo rumo/arfagem/taxas ->
   // erro de rumo e de elevacao ~0 -> comando ~0.
   const auto cmd = domain::pursuit(1000.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
   EXPECT_NEAR(cmd.rollNorm, 0.0, 1e-6);
   EXPECT_NEAR(cmd.pitchNorm, 0.0, 1e-6);
}

TEST(Guidance, AlvoADireitaBancaParaDireita)
{
   // Alvo a leste (relEastM > 0) do missil apontando para o norte -> erro de
   // rumo positivo -> banco positivo (direita).
   const auto cmd = domain::pursuit(0.0, 1000.0, 0.0, 0.0, 0.0, 0.0, 0.0);
   EXPECT_GT(cmd.rollNorm, 0.0);
}

TEST(Guidance, AlvoAcimaComandaCabrar)
{
   // relDownM < 0 quer dizer alvo ACIMA (convencao NED) -> elevacao positiva
   // -> comando de profundor positivo (cabrar).
   const auto cmd = domain::pursuit(1000.0, 0.0, -500.0, 0.0, 0.0, 0.0, 0.0);
   EXPECT_GT(cmd.pitchNorm, 0.0);
}

TEST(Guidance, TaxaPropriaAmortece)
{
   // Erro de rumo pequeno o bastante para nao saturar o comando (senao o
   // clamp escondera o efeito da taxa). Com taxa propria de rolagem JA no
   // sentido do comando, o termo derivativo tem de REDUZIR a magnitude do
   // comando -- e o amortecimento documentado em Guidance.hpp (ver a
   // armadilha do controlador so-proporcional).
   const auto semTaxa = domain::pursuit(1000.0, 450.0, 0.0, 0.0, 0.0, 0.0, 0.0);
   const auto comTaxa = domain::pursuit(1000.0, 450.0, 0.0, 0.0, 0.0, 30.0, 0.0);
   ASSERT_LT(std::abs(semTaxa.rollNorm), 1.0);   // pre-condicao: sem saturacao
   EXPECT_LT(comTaxa.rollNorm, semTaxa.rollNorm);
}

TEST(Guidance, ComandoSaturaEmMenosUmAUm)
{
   // Erro de rumo bem maior que o ganho de saturacao -> comando no limite,
   // nunca fora de [-1, 1].
   const auto cmd = domain::pursuit(-1.0, 1000.0, 0.0, 0.0, 0.0, 0.0, 0.0);
   EXPECT_LE(cmd.rollNorm, 1.0);
   EXPECT_GE(cmd.rollNorm, -1.0);
}

// slewTowards() -- o amortecimento que falta na lei P pura acima (ver o
// "porque" no header): limita a TAXA de variacao do comando, nao so a
// amplitude. GuidedMissile::guide() chama isto uma vez por frame,
// realimentando o proprio resultado como 'current' no frame seguinte.

TEST(SlewTowards, DentroDoLimiteAlcancaOAlvoDireto)
{
   EXPECT_DOUBLE_EQ(domain::slewTowards(0.0, 0.3, 4.0), 0.3);
}

TEST(SlewTowards, ForaDoLimiteAvancaSoMaxDelta)
{
   // GuidedMissile::guide() chama com maxDelta = kMaxSlewPerSec * dt
   // (4.0 * 0.02 = 0.08 num frame de 50 Hz) -- um comando pedindo +1.0 de
   // uma vez so tem que avancar so 0.08 por frame.
   EXPECT_DOUBLE_EQ(domain::slewTowards(0.0, 1.0, 0.08), 0.08);
   EXPECT_DOUBLE_EQ(domain::slewTowards(0.0, -1.0, 0.08), -0.08);   // simetrico
}

TEST(SlewTowards, JaNoAlvoENoOp)
{
   EXPECT_DOUBLE_EQ(domain::slewTowards(0.5, 0.5, 0.08), 0.5);
}

TEST(SlewTowards, ConvergeAposChamadasRepetidas)
{
   // Mesmo padrao de uso real: chamada por frame, realimentando o
   // resultado. 1.0 / 0.08 = 12.5 -> converge exatamente no 13o frame.
   double current{0.0};
   int frames{};
   while (current < 1.0 && frames < 100) {
      current = domain::slewTowards(current, 1.0, 0.08);
      frames++;
   }
   EXPECT_DOUBLE_EQ(current, 1.0);
   EXPECT_EQ(frames, 13);
}
