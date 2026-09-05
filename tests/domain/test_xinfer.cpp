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
#include "xrlbridge/ObservationFields.hpp"

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <vector>
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
   {
      // mkstemp e nao tmpnam: o segundo e uma corrida entre gerar o nome e
      // abrir, e o linker avisa sobre ele em toda compilacao.
      char molde[]{"/tmp/xinfer-teste-XXXXXX"};
      const int fd{::mkstemp(molde)};
      if (fd >= 0) ::close(fd);
      caminho_ = molde;
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


//------------------------------------------------------------------------------
// O CAMINHO FELIZ, contra o .onnx que o modelo de fato instala.
//
// POLICY_ONNX vem do meson (models/A4/configs/policy_example.onnx). Usar o
// arquivo DE VERDADE, e nao um gerado pelo teste, e o que faz estes casos
// valerem: eles quebram se o contrato derivar -- se alguem acrescentar um
// campo em ObservationFields.hpp sem reexportar o modelo, a forma deixa de
// bater e o teste acusa, em vez de a aeronave voar errado em silencio.
//------------------------------------------------------------------------------

TEST(XInfer, PoliticaInstaladaTemAFormaDoContrato)
{
   const xinfer::ModelId id{xinfer::open(POLICY_ONNX)};
   ASSERT_NE(id, 0) << "nao abriu " << POLICY_ONNX;

   int nIn{}, nOut{};
   ASSERT_TRUE(xinfer::shape(id, nIn, nOut));
   EXPECT_EQ(nIn, XRLBRIDGE_OBSERVATION_SIZE)
      << "a politica espera " << nIn << " entradas, mas a observacao canonica tem "
      << XRLBRIDGE_OBSERVATION_SIZE << " -- reexporte com src/rl/tools/export_onnx.py";
   EXPECT_EQ(nOut, XRLBRIDGE_ACTION_SIZE);
}

TEST(XInfer, MesmoCaminhoDevolveOMesmoId)
{
   // O cache e por caminho: quatro avioes apontando para o mesmo .onnx tem de
   // compartilhar UMA sessao. Sem isso seriam 4 x 9 ms de carga, o que nao
   // cabe num frame de 20 ms.
   EXPECT_EQ(xinfer::open(POLICY_ONNX), xinfer::open(POLICY_ONNX));
}

TEST(XInfer, InferenciaEhDeterministicaEmMilRepeticoes)
{
   const xinfer::ModelId id{xinfer::open(POLICY_ONNX)};
   ASSERT_NE(id, 0);

   std::array<float, XRLBRIDGE_OBSERVATION_SIZE> entrada{};
   for (std::size_t i = 0; i < entrada.size(); ++i) {
      entrada[i] = 0.1F * static_cast<float>(i);
   }

   std::array<float, XRLBRIDGE_ACTION_SIZE> referencia{};
   ASSERT_EQ(xinfer::run(id, entrada.data(), static_cast<int>(entrada.size()),
                         referencia.data(), static_cast<int>(referencia.size())),
             XRLBRIDGE_ACTION_SIZE);

   // Comparacao BIT A BIT, nao por tolerancia: o dump deterministico do
   // check-multi-thread compara com setprecision(9), entao qualquer diferenca
   // de ultimo bit acabaria aparecendo la.
   for (int repeticao = 0; repeticao < 1000; ++repeticao) {
      std::array<float, XRLBRIDGE_ACTION_SIZE> saida{};
      ASSERT_EQ(xinfer::run(id, entrada.data(), static_cast<int>(entrada.size()),
                            saida.data(), static_cast<int>(saida.size())),
                XRLBRIDGE_ACTION_SIZE);
      for (std::size_t i = 0; i < saida.size(); ++i) {
         ASSERT_EQ(std::memcmp(&saida[i], &referencia[i], sizeof(float)), 0)
            << "divergiu na repeticao " << repeticao << ", saida " << i;
      }
   }
}

TEST(XInfer, QuatroThreadsNaMesmaSessaoDaoOMesmoResultado)
{
   // O caso REAL da poc multi-thread: os quatro falcons decidem na fase 3, um
   // por thread do pool, compartilhando a MESMA sessao. Se isto divergisse, o
   // check-multi-thread quebraria -- e a causa seria dificil de achar la.
   const xinfer::ModelId id{xinfer::open(POLICY_ONNX)};
   ASSERT_NE(id, 0);

   std::array<float, XRLBRIDGE_OBSERVATION_SIZE> entrada{};
   for (std::size_t i = 0; i < entrada.size(); ++i) {
      entrada[i] = 0.1F * static_cast<float>(i);
   }

   std::array<float, XRLBRIDGE_ACTION_SIZE> referencia{};
   ASSERT_EQ(xinfer::run(id, entrada.data(), static_cast<int>(entrada.size()),
                         referencia.data(), static_cast<int>(referencia.size())),
             XRLBRIDGE_ACTION_SIZE);

   constexpr int kThreads{4};
   std::atomic<int> divergencias{};
   std::vector<std::thread> threads;
   threads.reserve(kThreads);
   for (int t = 0; t < kThreads; ++t) {
      threads.emplace_back([&] {
         for (int i = 0; i < 200; ++i) {
            std::array<float, XRLBRIDGE_ACTION_SIZE> saida{};
            if (xinfer::run(id, entrada.data(), static_cast<int>(entrada.size()),
                            saida.data(), static_cast<int>(saida.size()))
                != XRLBRIDGE_ACTION_SIZE) {
               divergencias.fetch_add(1);
               continue;
            }
            if (std::memcmp(saida.data(), referencia.data(),
                            sizeof(float) * saida.size()) != 0) {
               divergencias.fetch_add(1);
            }
         }
      });
   }
   for (auto& th : threads) th.join();
   EXPECT_EQ(divergencias.load(), 0)
      << "a mesma entrada deu saidas diferentes entre threads";
}

} // namespace
