//
// shared/xinfer -- o motor de inferencia, na camada mais isolada possivel:
// sem Station, sem player, sem plugin. So a lib e um arquivo em disco.
//
// O que ESTE arquivo cobre e a DEGRADACAO. Nenhuma das entradas aqui e um
// .onnx valido, e essa e a questao: um modelo ausente ou quebrado nao pode
// derrubar a simulacao no meio do frame -- tem de devolver 0 e deixar o
// consumidor decidir, exatamente como o joystick ausente cai pro Autopilot
// (shared/xjoystick) e a arvore que nao carrega vira nullptr
// (ubf/BtBehavior). O caminho FELIZ (inferencia de verdade, determinismo com
// 1/2/4 threads) exige um .onnx e mora em 'xinfer-determinismo'.
//
#include "xinfer/Infer.hpp"

#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <string>

namespace {

using namespace mixr;

// Um arquivo temporario com conteudo arbitrario -- o suficiente para provar
// que "existe em disco" nao e o mesmo que "e um modelo".
class ArquivoTemporario
{
public:
   explicit ArquivoTemporario(const std::string& conteudo)
      : caminho_{std::tmpnam(nullptr)}
   {
      std::ofstream out{caminho_, std::ios::binary};
      out << conteudo;
   }
   ~ArquivoTemporario() { std::remove(caminho_.c_str()); }

   const std::string& caminho() const { return caminho_; }

private:
   std::string caminho_;
};

TEST(XInfer, CaminhoVazioDevolveZero)
{
   EXPECT_EQ(xinfer::open(""), 0);
}

TEST(XInfer, ArquivoInexistenteDevolveZeroSemAbortar)
{
   EXPECT_EQ(xinfer::open("/nao/existe/politica.onnx"), 0);
}

TEST(XInfer, ArquivoQueNaoEhOnnxDevolveZeroSemAbortar)
{
   const ArquivoTemporario lixo{"isto nao e um protobuf de ModelProto"};
   EXPECT_EQ(xinfer::open(lixo.caminho()), 0);
}

// Chamar a mesma coisa duas vezes tem de dar o mesmo resultado -- o cache nao
// pode "aprender" um id para um caminho que falhou.
TEST(XInfer, FalhaNaoEntraNoCache)
{
   EXPECT_EQ(xinfer::open("/nao/existe/politica.onnx"), 0);
   EXPECT_EQ(xinfer::open("/nao/existe/politica.onnx"), 0);
}

TEST(XInfer, ShapeDeIdInvalidoDevolveFalso)
{
   int nIn{-1}, nOut{-1};
   EXPECT_FALSE(xinfer::shape(0, nIn, nOut));
   EXPECT_FALSE(xinfer::shape(-1, nIn, nOut));
   EXPECT_FALSE(xinfer::shape(9999, nIn, nOut));
}

// O contrato do valor de retorno: NEGATIVO em falha, nunca excecao para fora.
// E o que permite ao no da arvore devolver FAILURE e deixar o Fallback cair
// no ramo seguinte, em vez de derrubar o frame.
TEST(XInfer, RunComIdInvalidoDevolveNegativo)
{
   const float entrada[4]{0.0F, 0.0F, 0.0F, 0.0F};
   float saida[4]{};
   EXPECT_LT(xinfer::run(0, entrada, 4, saida, 4), 0);
   EXPECT_LT(xinfer::run(9999, entrada, 4, saida, 4), 0);
}

TEST(XInfer, RunComPonteiroNuloOuTamanhoZeroDevolveNegativo)
{
   const float entrada[4]{};
   float saida[4]{};
   EXPECT_LT(xinfer::run(1, nullptr, 4, saida, 4), 0);
   EXPECT_LT(xinfer::run(1, entrada, 4, nullptr, 4), 0);
   EXPECT_LT(xinfer::run(1, entrada, 0, saida, 4), 0);
   EXPECT_LT(xinfer::run(1, entrada, 4, saida, 0), 0);
}

} // namespace
