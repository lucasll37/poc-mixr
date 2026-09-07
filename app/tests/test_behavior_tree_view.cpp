#include "app/BehaviorTreeView.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

// O parser minimo de XML do BT.CPP (so o suficiente pro formato dele --
// ver o cabecalho do .cpp) e as duas regras puras que o consomem
// (matchesLabel()/flattenBehaviorTree()). Nenhuma linha daqui toca MIXR ou
// FTXUI -- so string/arquivo. Ate agora este arquivo so era exercitado
// INDIRETAMENTE (linkado por test_map_canvas_fit.cpp/
// test_component_tree_layout.cpp para satisfazer o link de MapPanel.cpp/
// ComponentTreePanel.cpp), sem nenhuma asserção sobre a logica dele.

namespace {

namespace fs = std::filesystem;

class BehaviorTreeViewTest : public ::testing::Test {
protected:
   fs::path dir;

   void SetUp() override
   {
      dir = fs::temp_directory_path()
          / ("bt_view_test_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed())
                              + "_" + ::testing::UnitTest::GetInstance()->current_test_info()->name());
      std::error_code ec;
      fs::remove_all(dir, ec);
      fs::create_directories(dir);
   }

   void TearDown() override
   {
      std::error_code ec;
      fs::remove_all(dir, ec);
   }

   fs::path writeFile(const std::string& name, const std::string& content) const
   {
      const fs::path path{dir / name};
      std::ofstream{path} << content;
      return path;
   }
};

// Mesmos bytes de app/BehaviorTreeView.cpp (U+251C/U+2514/U+2502, box
// drawing) -- copiados aqui, nao incluidos, porque sao internos ao .cpp
// (namespace anonimo).
const char* const kTee{"\xE2\x94\x9C\xE2\x94\x80\xE2\x94\x80 "};      // "├── "
const char* const kElbow{"\xE2\x94\x94\xE2\x94\x80\xE2\x94\x80 "};    // "└── "
const char* const kBar{"\xE2\x94\x82   "};                            // "│   "

} // namespace

//------------------------------------------------------------------------------
// isValid()
//------------------------------------------------------------------------------
TEST(BtNodeValidity, TagVaziaEhInvalidoTagPreenchidaEhValido)
{
   EXPECT_FALSE(app::isValid(app::BtNode{}));

   app::BtNode node;
   node.tag = "Patrol";
   EXPECT_TRUE(app::isValid(node));
}

//------------------------------------------------------------------------------
// matchesLabel() -- contido-em-qualquer-direcao, sobre o nome normalizado
// (maiusculo, so alfanumerico).
//------------------------------------------------------------------------------
TEST(MatchesLabel, TagIgualAoRotuloBate)
{
   EXPECT_TRUE(app::matchesLabel("Patrol", "PATROL"));
}

TEST(MatchesLabel, RotuloContidoNaTagBate)
{
   EXPECT_TRUE(app::matchesLabel("SupportAlert", "SUPPORT"));
}

TEST(MatchesLabel, TagContidaNoRotuloBateNaOutraDirecao)
{
   EXPECT_TRUE(app::matchesLabel("Patrol", "PATROLLING"));
}

TEST(MatchesLabel, SemRelacaoTextualNaoBate)
{
   // Limite documentado: 'ReturnToBase' decide em runtime entre "RTB"/"HOME",
   // nenhum dos dois substring do nome da tag.
   EXPECT_FALSE(app::matchesLabel("ReturnToBase", "RTB"));
}

TEST(MatchesLabel, RotuloTracoOuVazioNuncaBate)
{
   EXPECT_FALSE(app::matchesLabel("Patrol", "--"));
   EXPECT_FALSE(app::matchesLabel("Patrol", ""));
}

TEST(MatchesLabel, TagVaziaNuncaBate)
{
   EXPECT_FALSE(app::matchesLabel("", "PATROL"));
}

//------------------------------------------------------------------------------
// parseBehaviorTreeXml()
//------------------------------------------------------------------------------
TEST_F(BehaviorTreeViewTest, ArquivoInexistenteDevolveNoInvalido)
{
   const app::BtNode node{app::parseBehaviorTreeXml((dir / "nao-existe.xml").string())};
   EXPECT_FALSE(app::isValid(node));
}

TEST_F(BehaviorTreeViewTest, LeEstruturaAninhada)
{
   const fs::path xml{writeFile("tree.xml", R"(
      <root>
      <BehaviorTree ID="MainTree">
         <Fallback name="root">
            <Sequence>
               <FuelLow margin="0.05"/>
               <ReturnToBase/>
            </Sequence>
            <Patrol/>
         </Fallback>
      </BehaviorTree>
      </root>
   )")};

   const app::BtNode root{app::parseBehaviorTreeXml(xml.string())};
   ASSERT_TRUE(app::isValid(root));
   EXPECT_EQ(root.tag, "Fallback");
   ASSERT_EQ(root.children.size(), 2u);

   const app::BtNode& sequence{root.children[0]};
   EXPECT_EQ(sequence.tag, "Sequence");
   ASSERT_EQ(sequence.children.size(), 2u);
   EXPECT_EQ(sequence.children[0].tag, "FuelLow");
   EXPECT_TRUE(sequence.children[0].children.empty());
   EXPECT_EQ(sequence.children[1].tag, "ReturnToBase");

   const app::BtNode& patrol{root.children[1]};
   EXPECT_EQ(patrol.tag, "Patrol");
   EXPECT_TRUE(patrol.children.empty());
}

TEST_F(BehaviorTreeViewTest, IgnoraComentariosEDeclaracaoXml)
{
   const fs::path xml{writeFile("tree.xml",
      "<?xml version=\"1.0\"?>\n"
      "<!-- comentario qualquer -->\n"
      "<root><BehaviorTree><Patrol/></BehaviorTree></root>\n")};

   const app::BtNode root{app::parseBehaviorTreeXml(xml.string())};
   ASSERT_TRUE(app::isValid(root));
   EXPECT_EQ(root.tag, "Patrol");
}

TEST_F(BehaviorTreeViewTest, RaizAutoFechadaDevolveNoInvalido)
{
   const fs::path xml{writeFile("tree.xml", "<root><BehaviorTree/></root>")};

   EXPECT_FALSE(app::isValid(app::parseBehaviorTreeXml(xml.string())));
}

TEST_F(BehaviorTreeViewTest, SemTagBehaviorTreeDevolveNoInvalido)
{
   const fs::path xml{writeFile("tree.xml", "<root><Fallback><Patrol/></Fallback></root>")};

   EXPECT_FALSE(app::isValid(app::parseBehaviorTreeXml(xml.string())));
}

//------------------------------------------------------------------------------
// loadTreeForScenario() -- acha 'treeFile: "..."' no '.generated.edl' e
// delega para parseBehaviorTreeXml().
//------------------------------------------------------------------------------
TEST_F(BehaviorTreeViewTest, AchaTreeFileNoEdlEParseiaAArvoreApontada)
{
   const fs::path xml{writeFile("flight_tree.xml",
      "<root><BehaviorTree><Fallback><Patrol/></Fallback></BehaviorTree></root>")};
   const fs::path edl{writeFile("scenario.generated.edl",
      "falcon1: ( Aircraft\n   behavior: ( BtBehavior treeFile: \"" + xml.string() + "\" ) )\n")};

   const app::BtNode root{app::loadTreeForScenario(edl.string())};
   ASSERT_TRUE(app::isValid(root));
   EXPECT_EQ(root.tag, "Fallback");
   ASSERT_EQ(root.children.size(), 1u);
   EXPECT_EQ(root.children[0].tag, "Patrol");
}

TEST_F(BehaviorTreeViewTest, SemTreeFileNoEdlDevolveNoInvalido)
{
   const fs::path edl{writeFile("scenario.generated.edl", "( Station )\n")};
   EXPECT_FALSE(app::isValid(app::loadTreeForScenario(edl.string())));
}

TEST_F(BehaviorTreeViewTest, EdlInexistenteDevolveNoInvalido)
{
   EXPECT_FALSE(app::isValid(app::loadTreeForScenario((dir / "nao-existe.edl").string())));
}

//------------------------------------------------------------------------------
// flattenBehaviorTree() -- pre-ordem achatada, com prefixo de arvore Unicode.
//------------------------------------------------------------------------------
TEST(FlattenBehaviorTree, ArvoreInvalidaDevolveListaVazia)
{
   EXPECT_TRUE(app::flattenBehaviorTree(app::BtNode{}).empty());
}

TEST(FlattenBehaviorTree, RaizFolhaSemFilhosVirUmaLinhaSemPrefixo)
{
   app::BtNode root;
   root.tag = "Patrol";

   const auto lines{app::flattenBehaviorTree(root)};
   ASSERT_EQ(lines.size(), 1u);
   EXPECT_EQ(lines[0].tag, "Patrol");
   EXPECT_EQ(lines[0].display, "Patrol");
   EXPECT_TRUE(lines[0].leaf);
}

TEST(FlattenBehaviorTree, PreOrdemComPrefixosDeGaleria)
{
   // Fallback
   //  |- Sequence
   //  |   |- FuelLow
   //  |   `- ReturnToBase
   //  `- Patrol
   app::BtNode fuelLow; fuelLow.tag = "FuelLow";
   app::BtNode rtb; rtb.tag = "ReturnToBase";
   app::BtNode sequence; sequence.tag = "Sequence"; sequence.children = {fuelLow, rtb};
   app::BtNode patrol; patrol.tag = "Patrol";
   app::BtNode root; root.tag = "Fallback"; root.children = {sequence, patrol};

   const auto lines{app::flattenBehaviorTree(root)};
   ASSERT_EQ(lines.size(), 5u);

   EXPECT_EQ(lines[0].tag, "Fallback");
   EXPECT_EQ(lines[0].display, "Fallback");
   EXPECT_FALSE(lines[0].leaf);

   EXPECT_EQ(lines[1].tag, "Sequence");
   EXPECT_EQ(lines[1].display, std::string(kTee) + "Sequence");
   EXPECT_FALSE(lines[1].leaf);

   EXPECT_EQ(lines[2].tag, "FuelLow");
   EXPECT_EQ(lines[2].display, std::string(kBar) + kTee + "FuelLow");
   EXPECT_TRUE(lines[2].leaf);

   EXPECT_EQ(lines[3].tag, "ReturnToBase");
   EXPECT_EQ(lines[3].display, std::string(kBar) + kElbow + "ReturnToBase");
   EXPECT_TRUE(lines[3].leaf);

   EXPECT_EQ(lines[4].tag, "Patrol");
   EXPECT_EQ(lines[4].display, std::string(kElbow) + "Patrol");
   EXPECT_TRUE(lines[4].leaf);
}
