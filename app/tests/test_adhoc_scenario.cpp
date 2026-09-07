#include "app/AdHocScenario.hpp"

#include <gtest/gtest.h>

// app::falconFleet()/app::adHocScenario() -- a entrada de '-f <arquivo>'.
// Puro (so std::filesystem::path::stem()), sem MIXR, sem disco: nenhum dos
// caminhos testados aqui precisa que o arquivo exista de verdade.

TEST(FalconFleet, TemOsQuatroNomesNaOrdemDeVoo)
{
   const auto& fleet{app::falconFleet()};
   ASSERT_EQ(fleet.size(), 4u);
   EXPECT_EQ(fleet[0], "falcon1");
   EXPECT_EQ(fleet[1], "falcon2");
   EXPECT_EQ(fleet[2], "falcon3");
   EXPECT_EQ(fleet[3], "falcon4");
}

TEST(AdHocScenario, ChaveSaiDoNomeDoArquivoSemDiretorioNemExtensaoDupla)
{
   // stem() tira uma extensao so -- sem o corte extra de '.edl' a chave
   // sairia 'scenario.edl' para as fixtures '<nome>.edl.in'.
   const app::ScenarioEntry e{app::adHocScenario("/a/b/c/scenario.edl.in")};
   EXPECT_EQ(e.key, "scenario");
   EXPECT_EQ(e.label, "scenario");
   EXPECT_EQ(e.templatePath, "/a/b/c/scenario.edl.in");
}

TEST(AdHocScenario, ChaveComExtensaoEdlSimplesNaoRepeteOCorte)
{
   const app::ScenarioEntry e{app::adHocScenario("configs/scenario.edl")};
   EXPECT_EQ(e.key, "scenario");
}

TEST(AdHocScenario, ChaveSemExtensaoMantemONomeInteiro)
{
   const app::ScenarioEntry e{app::adHocScenario("configs/cenario_sem_extensao")};
   EXPECT_EQ(e.key, "cenario_sem_extensao");
}

TEST(AdHocScenario, CaminhoVazioUsaOFallbackAdHoc)
{
   const app::ScenarioEntry e{app::adHocScenario("")};
   EXPECT_EQ(e.key, "ad-hoc");
   EXPECT_EQ(e.label, "ad-hoc");
}

TEST(AdHocScenario, FrotaEhSempreAFalconFleet)
{
   const app::ScenarioEntry e{app::adHocScenario("qualquer.edl")};
   EXPECT_EQ(e.fleet, app::falconFleet());
}

TEST(AdHocScenario, TokensDeTacviewFicamVaziosEDescricaoEFixa)
{
   const app::ScenarioEntry e{app::adHocScenario("qualquer.edl")};
   EXPECT_TRUE(e.tacviewId.empty());
   EXPECT_TRUE(e.tacviewModelMap.empty());
   EXPECT_TRUE(e.tacviewTypeMap.empty());
   EXPECT_TRUE(e.tacviewColorMap.empty());
   EXPECT_EQ(e.description, "cenario carregado por -f");
}
