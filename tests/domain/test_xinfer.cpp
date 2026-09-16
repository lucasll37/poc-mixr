//
// libs/xinfer -- o motor de inferencia, na camada mais isolada possivel:
// sem Station, sem player, sem plugin. So a lib e um arquivo em disco.
//
// A PRIMEIRA METADE deste arquivo cobre a DEGRADACAO. Nenhuma das entradas
// nesses casos e um .onnx valido, e essa e a questao: um modelo ausente ou
// quebrado nao pode derrubar a simulacao no meio do frame -- tem de devolver
// 0 e deixar o consumidor decidir, exatamente como o joystick ausente cai
// pro Autopilot (libs/xjoystick) e a arvore que nao carrega vira nullptr
// (ubf/BtBehavior).
//
// A SEGUNDA METADE (a partir de PoliticaInstaladaTemAFormaDoContrato, mais
// abaixo neste mesmo arquivo) e o caminho FELIZ -- inferencia de verdade
// contra POLICY_ONNX (um .onnx real, ver a definicao de compilacao em
// tests/meson.build) e determinismo com 1/2/4 threads sobre a MESMA sessao,
// sob o unico alvo 'xinfer-degradacao' registrado em tests/meson.build.
//
#include "xinfer/Infer.hpp"
#include "xrlbridge/ObservationFields.hpp"
#include "xrlbridge/RLBridge.hpp"

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
// POLICY_ONNX vem do meson (models/players/air/A-4/configs/policy_example.onnx). Usar o
// arquivo DE VERDADE, e nao um gerado pelo teste, e o que faz estes casos
// valerem: eles quebram se o contrato derivar -- se alguem acrescentar um
// campo em ObservationFields.hpp sem reexportar o modelo, a forma deixa de
// bater e o teste acusa, em vez de a aeronave voar errado em silencio.
//
// A forma esperada e' a de xrlbridge::classicSchema28() (28), NAO de
// XRLBRIDGE_OBSERVATION_SIZE (38 desde que a macro canonica cresceu para
// expor RWR/navegacao aos nos de arvore): POLICY_ONNX foi exportado contra o
// schema "classic28" (o default de export_onnx.py), nunca contra a lista
// completa.
//------------------------------------------------------------------------------

TEST(XInfer, PoliticaInstaladaTemAFormaDoContrato)
{
   const xinfer::ModelId id{xinfer::open(POLICY_ONNX)};
   ASSERT_NE(id, 0) << "nao abriu " << POLICY_ONNX;

   const int nEsperado{static_cast<int>(xrlbridge::classicSchema28().fieldNames.size())};

   int nIn{}, nOut{};
   ASSERT_TRUE(xinfer::shape(id, nIn, nOut));
   EXPECT_EQ(nIn, nEsperado)
      << "a politica espera " << nIn << " entradas, mas o schema 'classic28' tem "
      << nEsperado << " -- reexporte com src/poc/rl-training/tools/export_onnx.py";
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

   // POLICY_ONNX espera o tamanho de classicSchema28() (28), nao o da lista
   // canonica completa (38) -- ver o comentario acima de
   // PoliticaInstaladaTemAFormaDoContrato.
   std::vector<float> entrada(xrlbridge::classicSchema28().fieldNames.size());
   for (std::size_t i = 0; i < entrada.size(); ++i) {
      entrada[i] = 0.1F * static_cast<float>(i);
   }

   std::array<float, XRLBRIDGE_ACTION_SIZE> referencia{};
   ASSERT_EQ(xinfer::run(id, entrada.data(), static_cast<int>(entrada.size()),
                         referencia.data(), static_cast<int>(referencia.size())),
             XRLBRIDGE_ACTION_SIZE);

   // Comparacao BIT A BIT, nao por tolerancia: o dump deterministico da poc
   // flight compara com setprecision(9), entao qualquer diferenca de ultimo
   // bit acabaria aparecendo la.
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
   // O caso REAL da poc flight: os quatro falcons decidem na fase 3, um
   // por thread do pool, compartilhando a MESMA sessao. Se isto divergisse, o
   // determinismo da poc quebraria -- e a causa seria dificil de achar la.
   const xinfer::ModelId id{xinfer::open(POLICY_ONNX)};
   ASSERT_NE(id, 0);

   // POLICY_ONNX espera o tamanho de classicSchema28() (28) -- ver o
   // comentario de PoliticaInstaladaTemAFormaDoContrato, acima.
   std::vector<float> entrada(xrlbridge::classicSchema28().fieldNames.size());
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

//------------------------------------------------------------------------------
// fields() -- a metadata 'xrlbridge.fields' (peca nova desta passada: fecha
// o risco que schema variavel introduz -- dois .onnx do MESMO tamanho podem
// esperar campos DIFERENTES, e so a checagem de contagem nao pegaria isso).
//------------------------------------------------------------------------------

TEST(XInfer, FieldsDeIdInvalidoDevolveFalso)
{
   std::vector<std::string> nomes;
   EXPECT_FALSE(xinfer::fields(0, nomes));
   EXPECT_FALSE(xinfer::fields(-1, nomes));
   EXPECT_FALSE(xinfer::fields(9999, nomes));
}

// POLICY_ONNX foi exportado ANTES desta funcionalidade existir -- sem a
// metadata. O contrato e' devolver 'false', nao um vetor vazio-mas-'true'
// (que o chamador confundiria com "0 campos", nao "sem info disponivel").
TEST(XInfer, FieldsDeOnnxSemMetadataDevolveFalso)
{
   const xinfer::ModelId id{xinfer::open(POLICY_ONNX)};
   ASSERT_NE(id, 0);
   std::vector<std::string> nomes;
   EXPECT_FALSE(xinfer::fields(id, nomes));
}

// POLICY_ALL38_ONNX foi exportado com --fields all -- a metadata tem que
// devolver os 38 nomes, NA ORDEM exata (identidade, nao so contagem).
TEST(XInfer, FieldsDeOnnxComMetadataDevolveOsNomesNaOrdemExata)
{
   const xinfer::ModelId id{xinfer::open(POLICY_ALL38_ONNX)};
   ASSERT_NE(id, 0);

   std::vector<std::string> nomes;
   ASSERT_TRUE(xinfer::fields(id, nomes));

   const auto esperado = mixr::xrlbridge::observationFieldNames();
   EXPECT_EQ(nomes, esperado);
}

} // namespace
