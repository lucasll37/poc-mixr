// A logica de janela deslizante + veredito de "crescimento sustentado" da
// aba Memoria do dashboard (app/MetaObjectSnapshot.hpp) -- pura, sem MIXR
// nem FTXUI, alimentada aqui com sequencias sinteticas de 'count'. O que se
// trava e o CRITERIO: contagem que so cresce (nunca cai dentro da janela)
// vira suspeita; contagem que oscila ou se estabiliza, nao.

#include "app/MetaObjectSnapshot.hpp"

#include <gtest/gtest.h>

namespace {

using app::ClassStat;
using app::kHistoryWindow;
using app::updateClassStat;

// Alimenta 'sequence.size()' amostras (uma por chamada de updateClassStat,
// carregando o historico entre chamadas) e devolve o ultimo ClassStat.
ClassStat feed(const std::vector<int>& sequence)
{
   ClassStat s;
   for (const int count : sequence) {
      s = updateClassStat(s, "Classe", true, count, count, count);
   }
   return s;
}

std::vector<int> constante(const int v, const int n) { return std::vector<int>(n, v); }

TEST(MetaObjectSnapshot, JanelaIncompletaNuncaAcusa)
{
   // Menos amostras que kHistoryWindow -- nao ha veredito ainda, mesmo
   // crescendo sem parar.
   std::vector<int> seq;
   for (int i = 0; i < kHistoryWindow - 1; i++) seq.push_back(i);
   EXPECT_FALSE(feed(seq).suspectedLeak);
}

TEST(MetaObjectSnapshot, CrescimentoSustentadoAcusa)
{
   // count sobe 1 a cada amostra, nunca cai -- exatamente o padrao de um
   // vazamento: instancias criadas e nunca destruidas.
   std::vector<int> seq;
   for (int i = 0; i < kHistoryWindow + 10; i++) seq.push_back(i);
   EXPECT_TRUE(feed(seq).suspectedLeak);
}

TEST(MetaObjectSnapshot, ContagemEstavelNaoAcusa)
{
   EXPECT_FALSE(feed(constante(42, kHistoryWindow + 5)).suspectedLeak);
}

TEST(MetaObjectSnapshot, CriaEDestroiEmEquilibrioNaoAcusa)
{
   // Sobe e desce dentro da janela -- o padrao normal de um combate
   // (misseis lancados e detonados) nao pode acender o alarme.
   std::vector<int> seq;
   for (int i = 0; i < kHistoryWindow + 20; i++) seq.push_back((i % 2 == 0) ? 5 : 3);
   EXPECT_FALSE(feed(seq).suspectedLeak);
}

TEST(MetaObjectSnapshot, QuedaNoMeioInvalidaMesmoTerminandoMaisAlto)
{
   // Sobe, cai UMA vez no meio, sobe de novo -- termina bem mais alto que
   // comecou (grew==true), mas nao e crescimento SUSTENTADO: a queda no
   // meio (destruicao em lote) tem de derrubar o veredito.
   std::vector<int> seq;
   for (int i = 0; i < kHistoryWindow; i++) {
      if (i < 20) seq.push_back(i);            // 0..19, subindo
      else if (i == 20) seq.push_back(15);      // queda
      else seq.push_back(i - 5);                // 16..24, subindo de novo
   }
   ASSERT_LT(seq.front(), seq.back());   // confirma que termina mais alto
   EXPECT_FALSE(feed(seq).suspectedLeak);
}

TEST(MetaObjectSnapshot, JanelaDeslizaEDescartaAmostraAntiga)
{
   // Cresce alem da janela, depois se estabiliza por kHistoryWindow
   // amostras -- a janela deve "esquecer" o crescimento antigo.
   std::vector<int> seq;
   for (int i = 0; i < kHistoryWindow; i++) seq.push_back(i);
   for (int i = 0; i < kHistoryWindow; i++) seq.push_back(kHistoryWindow - 1);
   EXPECT_FALSE(feed(seq).suspectedLeak);
}

TEST(MetaObjectSnapshot, CamposBasicosSaoRepassados)
{
   const ClassStat s{updateClassStat(ClassStat{}, "GuidedMissile", true, 3, 7, 42)};
   EXPECT_EQ(s.factoryName, "GuidedMissile");
   EXPECT_TRUE(s.fromPlugin);
   EXPECT_EQ(s.count, 3);
   EXPECT_EQ(s.mc, 7);
   EXPECT_EQ(s.tc, 42);
}

} // namespace
