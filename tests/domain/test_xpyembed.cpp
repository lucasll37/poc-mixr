//
// shared/xpyembed -- o interpretador Python embarcado, sem Station e sem
// plugin: so a lib e um script em disco.
//
// O QUE IMPORTA AQUI e o isolamento por chave. O GIL serializa as chamadas,
// mas a ORDEM em que as threads o adquirem nao e deterministica -- entao a
// unica coisa que mantem o check-multi-thread verde e cada aeronave ter o SEU
// dicionario de globais. Estes testes travam exatamente isso.
//
#include "xpyembed/PyEmbed.hpp"

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <cstdio>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace mixr;

class ScriptTemporario
{
public:
   explicit ScriptTemporario(const std::string& fonte) : caminho_{std::tmpnam(nullptr)}
   {
      caminho_ += ".py";
      std::ofstream out{caminho_};
      out << fonte;
   }
   ~ScriptTemporario() { std::remove(caminho_.c_str()); }
   const std::string& caminho() const { return caminho_; }

private:
   std::string caminho_;
};

// Sem Python no sistema NADA aqui pode falhar de forma ruidosa -- a lib tem de
// dizer "indisponivel" e o consumidor degrada. Por isso quase todo teste
// comeca por este gate.
bool temPython() { return xpyembed::isAvailable(); }

TEST(XPyEmbed, ScriptInexistenteDevolveZero)
{
   EXPECT_EQ(xpyembed::loadScript("/nao/existe/politica.py"), 0);
}

TEST(XPyEmbed, CaminhoVazioDevolveZero)
{
   EXPECT_EQ(xpyembed::loadScript(""), 0);
}

TEST(XPyEmbed, DecideComIdInvalidoDevolveFalso)
{
   const std::array<double, 4> obs{};
   std::array<double, 3> cmd{};
   EXPECT_FALSE(xpyembed::decide(0, 1, obs.data(), 4, cmd.data(), 3));
   EXPECT_FALSE(xpyembed::decide(9999, 1, obs.data(), 4, cmd.data(), 3));
}

TEST(XPyEmbed, ScriptSemDecideDevolveZero)
{
   if (!temPython()) GTEST_SKIP() << "sem interpretador Python";
   const ScriptTemporario s{"x = 1\n"};
   const auto id = xpyembed::loadScript(s.caminho());
   ASSERT_NE(id, 0) << "o arquivo existe, entao carregar tem de dar certo";
   // A ausencia de decide() so e detectada na PRIMEIRA chamada, quando o
   // script e executado no dicionario do player.
   const std::array<double, 3> obs{};
   std::array<double, 3> cmd{};
   EXPECT_FALSE(xpyembed::decide(id, 1, obs.data(), 3, cmd.data(), 3));
}

TEST(XPyEmbed, ExcecaoNoScriptDevolveFalsoSemAbortar)
{
   if (!temPython()) GTEST_SKIP() << "sem interpretador Python";
   const ScriptTemporario s{"def decide(obs):\n    raise ValueError('de proposito')\n"};
   const auto id = xpyembed::loadScript(s.caminho());
   ASSERT_NE(id, 0);
   const std::array<double, 3> obs{};
   std::array<double, 3> cmd{};
   EXPECT_FALSE(xpyembed::decide(id, 1, obs.data(), 3, cmd.data(), 3));
}

TEST(XPyEmbed, RetornoCurtoDemaisDevolveFalso)
{
   if (!temPython()) GTEST_SKIP() << "sem interpretador Python";
   const ScriptTemporario s{"def decide(obs):\n    return (1.0, 2.0)\n"};
   const auto id = xpyembed::loadScript(s.caminho());
   ASSERT_NE(id, 0);
   const std::array<double, 3> obs{};
   std::array<double, 3> cmd{};
   EXPECT_FALSE(xpyembed::decide(id, 1, obs.data(), 3, cmd.data(), 3));
}

TEST(XPyEmbed, DecideDevolveOQueOScriptCalculou)
{
   if (!temPython()) GTEST_SKIP() << "sem interpretador Python";
   const ScriptTemporario s{
      "def decide(obs):\n"
      "    return (obs[0] * 2.0, obs[1] + 10.0, sum(obs))\n"};
   const auto id = xpyembed::loadScript(s.caminho());
   ASSERT_NE(id, 0);

   const std::array<double, 3> obs{1.5, 2.5, 3.0};
   std::array<double, 3> cmd{};
   ASSERT_TRUE(xpyembed::decide(id, 1, obs.data(), 3, cmd.data(), 3));
   EXPECT_DOUBLE_EQ(cmd[0], 3.0);
   EXPECT_DOUBLE_EQ(cmd[1], 12.5);
   EXPECT_DOUBLE_EQ(cmd[2], 7.0);
}

TEST(XPyEmbed, MesmoCaminhoDevolveOMesmoId)
{
   if (!temPython()) GTEST_SKIP() << "sem interpretador Python";
   const ScriptTemporario s{"def decide(obs):\n    return (0.0, 0.0, 0.0)\n"};
   EXPECT_EQ(xpyembed::loadScript(s.caminho()), xpyembed::loadScript(s.caminho()));
}

// O TESTE QUE JUSTIFICA O DESENHO. Um script com estado em nivel de modulo,
// rodado por duas chaves diferentes: se os globais fossem compartilhados, o
// contador de um vazaria no outro e o resultado dependeria da ordem.
TEST(XPyEmbed, CadaChaveTemOSeuProprioEstado)
{
   if (!temPython()) GTEST_SKIP() << "sem interpretador Python";
   const ScriptTemporario s{
      "contador = 0\n"
      "def decide(obs):\n"
      "    global contador\n"
      "    contador += 1\n"
      "    return (float(contador), 0.0, 0.0)\n"};
   const auto id = xpyembed::loadScript(s.caminho());
   ASSERT_NE(id, 0);

   const std::array<double, 1> obs{};
   std::array<double, 3> cmd{};

   // player 1 decide tres vezes
   for (int i = 1; i <= 3; ++i) {
      ASSERT_TRUE(xpyembed::decide(id, 1, obs.data(), 1, cmd.data(), 3));
      EXPECT_DOUBLE_EQ(cmd[0], static_cast<double>(i));
   }
   // player 2 comeca do zero -- e o ponto
   ASSERT_TRUE(xpyembed::decide(id, 2, obs.data(), 1, cmd.data(), 3));
   EXPECT_DOUBLE_EQ(cmd[0], 1.0) << "o estado do player 1 vazou para o player 2";
}

// O caso real: quatro aeronaves decidindo em paralelo, cada uma com a sua
// chave. Cada uma tem de ver o SEU proprio contador, e o total tem de fechar.
TEST(XPyEmbed, QuatroThreadsComChavesDistintasNaoSeMisturam)
{
   if (!temPython()) GTEST_SKIP() << "sem interpretador Python";
   const ScriptTemporario s{
      "contador = 0\n"
      "def decide(obs):\n"
      "    global contador\n"
      "    contador += 1\n"
      "    return (float(contador), 0.0, 0.0)\n"};
   const auto id = xpyembed::loadScript(s.caminho());
   ASSERT_NE(id, 0);

   constexpr int kThreads{4};
   constexpr int kChamadas{200};
   std::atomic<int> erros{};
   std::vector<std::thread> threads;
   threads.reserve(kThreads);
   for (int t = 0; t < kThreads; ++t) {
      // chave 100+t: uma por "aeronave"
      threads.emplace_back([&, chave = 100 + t] {
         const std::array<double, 1> obs{};
         std::array<double, 3> cmd{};
         for (int i = 1; i <= kChamadas; ++i) {
            if (!xpyembed::decide(id, chave, obs.data(), 1, cmd.data(), 3)) {
               erros.fetch_add(1);
            } else if (cmd[0] != static_cast<double>(i)) {
               // Se os globais fossem compartilhados, este contador saltaria.
               erros.fetch_add(1);
            }
         }
      });
   }
   for (auto& th : threads) th.join();
   EXPECT_EQ(erros.load(), 0)
      << "o estado vazou entre chaves, ou uma chamada falhou sob concorrencia";
}

} // namespace
