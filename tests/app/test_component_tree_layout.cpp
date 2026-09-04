// A logica PURA por tras da aba "Componentes" (F6) depois da passada que a
// tornou VERTICAL e retratil: app/ComponentTreePanel.{hpp,cpp}.
//
// Sem Station, sem terminal: a arvore de entrada e montada a mao (o mesmo
// ComponentTreeNode que app::discoverComponentTree() produziria), e o que se
// afirma e o LAYOUT (a arvore cresce para baixo, o pai fica centrado sobre
// os filhos), o conjunto de RETRAIDOS (quem some do layout, quem sobrevive a
// uma redescoberta) e a NAVEGACAO por teclado -- as tres coisas que a
// passada acrescentou. Mesmo espirito de tests/app/test_map_canvas_fit.cpp:
// exercita o codigo de producao, nao uma reimplementacao dele.
#include "app/ComponentTreePanel.hpp"

#include <gtest/gtest.h>

#include <string>

namespace {

// Monta um no ja com a chave que discoverComponentTree() daria (o pai
// concatena a sua com o nome do filho -- ver makeNodeKey()).
app::ComponentTreeNode makeNode(const std::string& parentKey, const std::string& slot,
                                const std::string& cls = "Classe")
{
   app::ComponentTreeNode node;
   node.slotName = slot;
   node.className = cls;
   node.nodeKey = (parentKey == "/") ? ("/" + slot) : (parentKey + "/" + slot);
   return node;
}

//   /  (Station)
//   |- /plugins
//   |- /simulation
//   |   `- /simulation/players
//   |       |- .../falcon1  -> dynamicsModel, pilot
//   |       `- .../falcon2
//   `- /dataRecorder
app::ComponentTreeNode makeSampleTree()
{
   app::ComponentTreeNode root;
   root.nodeKey = "/";
   root.className = "Station";
   root.phase = app::EstimatedPhase::Structural;

   root.children.push_back(makeNode("/", "plugins"));

   app::ComponentTreeNode sim{makeNode("/", "simulation")};
   app::ComponentTreeNode players{makeNode(sim.nodeKey, "players")};

   app::ComponentTreeNode falcon1{makeNode(players.nodeKey, "falcon1", "Aircraft")};
   falcon1.isPlayer = true;
   falcon1.playerId = 101;
   falcon1.children.push_back(makeNode(falcon1.nodeKey, "dynamicsModel"));
   falcon1.children.push_back(makeNode(falcon1.nodeKey, "pilot"));

   app::ComponentTreeNode falcon2{makeNode(players.nodeKey, "falcon2", "Aircraft")};
   falcon2.isPlayer = true;
   falcon2.playerId = 102;

   players.children.push_back(std::move(falcon1));
   players.children.push_back(std::move(falcon2));
   sim.children.push_back(std::move(players));
   root.children.push_back(std::move(sim));

   root.children.push_back(makeNode("/", "dataRecorder"));
   return root;
}

const app::ComponentTreeLayoutNode& nodeByKey(const app::ComponentTreeLayout& layout,
                                              const std::string& key)
{
   const int index{app::findComponentNodeIndex(layout, key)};
   EXPECT_GE(index, 0) << "no ausente do layout: " << key;
   return layout.nodes[static_cast<std::size_t>(index)];
}

}   // namespace

//------------------------------------------------------------------------------
// Layout VERTICAL: profundidade vira LINHA (cresce para baixo) e a ordem
// entre irmaos vira COLUNA -- o oposto do desenho anterior, que crescia da
// esquerda para a direita.
//------------------------------------------------------------------------------
TEST(ComponentTreeLayout, ProfundidadeCresceParaBaixo)
{
   const app::ComponentTreeLayout layout{app::layoutComponentTree(makeSampleTree(), {})};

   const auto& root{nodeByKey(layout, "/")};
   const auto& sim{nodeByKey(layout, "/simulation")};
   const auto& players{nodeByKey(layout, "/simulation/players")};

   EXPECT_EQ(root.depth, 0);
   EXPECT_LT(root.y, sim.y);
   EXPECT_LT(sim.y, players.y);
}

TEST(ComponentTreeLayout, IrmaosCompartilhamALinhaEOcupamColunasDistintas)
{
   const app::ComponentTreeLayout layout{app::layoutComponentTree(makeSampleTree(), {})};

   const auto& plugins{nodeByKey(layout, "/plugins")};
   const auto& sim{nodeByKey(layout, "/simulation")};
   const auto& recorder{nodeByKey(layout, "/dataRecorder")};

   EXPECT_DOUBLE_EQ(plugins.y, sim.y);
   EXPECT_DOUBLE_EQ(sim.y, recorder.y);
   EXPECT_LT(plugins.x, sim.x);
   EXPECT_LT(sim.x, recorder.x);
}

TEST(ComponentTreeLayout, PaiFicaCentradoEntreOPrimeiroEOUltimoFilho)
{
   const app::ComponentTreeLayout layout{app::layoutComponentTree(makeSampleTree(), {})};

   const auto& falcon1{nodeByKey(layout, "/simulation/players/falcon1")};
   const auto& dyn{nodeByKey(layout, "/simulation/players/falcon1/dynamicsModel")};
   const auto& pilot{nodeByKey(layout, "/simulation/players/falcon1/pilot")};

   EXPECT_DOUBLE_EQ(falcon1.x, (dyn.x + pilot.x) / 2.0);
}

//------------------------------------------------------------------------------
// Retrair/expandir.
//------------------------------------------------------------------------------
TEST(ComponentTreeCollapse, GalhoRetraidoSomeDoLayoutMasContinuaContando)
{
   const app::ComponentTreeNode root{makeSampleTree()};
   app::CollapsedNodes collapsed{"/simulation/players/falcon1"};

   const app::ComponentTreeLayout layout{app::layoutComponentTree(root, collapsed)};

   const auto& falcon1{nodeByKey(layout, "/simulation/players/falcon1")};
   EXPECT_TRUE(falcon1.collapsed);
   // 'childCount' e o numero de filhos REAIS -- e o que o desenho usa pra
   // dizer "[+2]" e o que prova que ha o que expandir.
   EXPECT_EQ(falcon1.childCount, 2);

   EXPECT_LT(app::findComponentNodeIndex(layout, "/simulation/players/falcon1/dynamicsModel"), 0);
   EXPECT_LT(app::findComponentNodeIndex(layout, "/simulation/players/falcon1/pilot"), 0);
   // O IRMAO nao e afetado.
   EXPECT_GE(app::findComponentNodeIndex(layout, "/simulation/players/falcon2"), 0);
}

TEST(ComponentTreeCollapse, AlternarEIdempotenteEIgnoraFolha)
{
   const app::ComponentTreeNode root{makeSampleTree()};
   app::CollapsedNodes collapsed;
   const app::ComponentTreeLayout layout{app::layoutComponentTree(root, collapsed)};

   const auto& falcon1{nodeByKey(layout, "/simulation/players/falcon1")};
   app::toggleComponentNodeCollapsed(collapsed, falcon1);
   EXPECT_EQ(collapsed.count("/simulation/players/falcon1"), 1u);
   app::toggleComponentNodeCollapsed(collapsed, falcon1);
   EXPECT_EQ(collapsed.count("/simulation/players/falcon1"), 0u);

   // Retrair uma FOLHA nao significa nada -- nao pode sujar o conjunto.
   const auto& plugins{nodeByKey(layout, "/plugins")};
   app::toggleComponentNodeCollapsed(collapsed, plugins);
   EXPECT_TRUE(collapsed.empty());
}

TEST(ComponentTreeCollapse, RetrairTudoDeixaSoARaiz)
{
   const app::ComponentTreeNode root{makeSampleTree()};
   app::CollapsedNodes collapsed;
   app::collapseAllComponentNodes(root, collapsed);

   const app::ComponentTreeLayout layout{app::layoutComponentTree(root, collapsed)};
   ASSERT_EQ(layout.nodes.size(), 1u);
   EXPECT_EQ(layout.nodes.front().nodeKey, "/");
   EXPECT_TRUE(layout.nodes.front().collapsed);
   EXPECT_EQ(layout.nodes.front().childCount, 3);
}

TEST(ComponentTreeCollapse, ProfundidadeInicialMostraAteOsPlayers)
{
   const app::ComponentTreeNode root{makeSampleTree()};
   app::CollapsedNodes collapsed;
   app::collapseDeeperThan(root, app::kTreeInitialExpandDepth, collapsed);

   const app::ComponentTreeLayout layout{app::layoutComponentTree(root, collapsed)};

   // Os players em si aparecem (e o que responde "quem esta no cenario"),
   // mas a subarvore de cada um -- a parte larga -- nasce retraida.
   EXPECT_GE(app::findComponentNodeIndex(layout, "/simulation/players/falcon1"), 0);
   EXPECT_LT(app::findComponentNodeIndex(layout, "/simulation/players/falcon1/pilot"), 0);
}

TEST(ComponentTreeCollapse, ChaveSobreviveARedescobertaDaArvore)
{
   // A arvore e redescoberta a cada redesenho: o mesmo conjunto de chaves
   // tem de continuar valendo contra um objeto NOVO, e a selecao junto.
   app::CollapsedNodes collapsed{"/simulation/players/falcon1"};
   app::ComponentTreeViewState view;
   view.selectedKey = "/simulation/players/falcon2";

   const app::ComponentTreeLayout again{app::layoutComponentTree(makeSampleTree(), collapsed)};

   EXPECT_TRUE(nodeByKey(again, "/simulation/players/falcon1").collapsed);
   EXPECT_GE(app::findComponentNodeIndex(again, view.selectedKey), 0);
}

//------------------------------------------------------------------------------
// Navegacao por teclado -- e o que torna retrair/expandir usavel sem mouse.
//------------------------------------------------------------------------------
TEST(ComponentTreeNavigation, SemSelecaoQualquerDirecaoEntraPelaRaiz)
{
   const app::ComponentTreeLayout layout{app::layoutComponentTree(makeSampleTree(), {})};
   app::ComponentTreeViewState view;
   app::CollapsedNodes collapsed;

   EXPECT_TRUE(app::navigateComponentTree(layout, view, app::TreeNavigation::NextSibling, collapsed));
   EXPECT_EQ(view.selectedKey, "/");
}

TEST(ComponentTreeNavigation, DesceSobeEAndaEntreIrmaos)
{
   const app::ComponentTreeLayout layout{app::layoutComponentTree(makeSampleTree(), {})};
   app::ComponentTreeViewState view;
   app::CollapsedNodes collapsed;
   view.selectedKey = "/";

   EXPECT_TRUE(app::navigateComponentTree(layout, view, app::TreeNavigation::FirstChild, collapsed));
   EXPECT_EQ(view.selectedKey, "/plugins");

   EXPECT_TRUE(app::navigateComponentTree(layout, view, app::TreeNavigation::NextSibling, collapsed));
   EXPECT_EQ(view.selectedKey, "/simulation");
   EXPECT_TRUE(app::navigateComponentTree(layout, view, app::TreeNavigation::NextSibling, collapsed));
   EXPECT_EQ(view.selectedKey, "/dataRecorder");
   // Ultimo irmao: nao ha para onde ir, e a selecao nao pode "vazar" para o
   // no seguinte da pre-ordem (que e de outro pai).
   EXPECT_FALSE(app::navigateComponentTree(layout, view, app::TreeNavigation::NextSibling, collapsed));
   EXPECT_EQ(view.selectedKey, "/dataRecorder");

   EXPECT_TRUE(app::navigateComponentTree(layout, view, app::TreeNavigation::PrevSibling, collapsed));
   EXPECT_EQ(view.selectedKey, "/simulation");
   EXPECT_TRUE(app::navigateComponentTree(layout, view, app::TreeNavigation::Parent, collapsed));
   EXPECT_EQ(view.selectedKey, "/");
   EXPECT_FALSE(app::navigateComponentTree(layout, view, app::TreeNavigation::Parent, collapsed));
}

TEST(ComponentTreeNavigation, DescerParaDentroDeGalhoRetraidoExpandeEle)
{
   const app::ComponentTreeNode root{makeSampleTree()};
   app::CollapsedNodes collapsed{"/simulation/players/falcon1"};
   const app::ComponentTreeLayout layout{app::layoutComponentTree(root, collapsed)};

   app::ComponentTreeViewState view;
   view.selectedKey = "/simulation/players/falcon1";

   // O primeiro passo ABRE o galho (os filhos nem estao neste layout);
   // quem passa a selecao pro filho e o passo seguinte, ja no layout novo.
   EXPECT_TRUE(app::navigateComponentTree(layout, view, app::TreeNavigation::FirstChild, collapsed));
   EXPECT_EQ(collapsed.count("/simulation/players/falcon1"), 0u);
   EXPECT_EQ(view.selectedKey, "/simulation/players/falcon1");

   const app::ComponentTreeLayout aberto{app::layoutComponentTree(root, collapsed)};
   EXPECT_TRUE(app::navigateComponentTree(aberto, view, app::TreeNavigation::FirstChild, collapsed));
   EXPECT_EQ(view.selectedKey, "/simulation/players/falcon1/dynamicsModel");
}

TEST(ComponentTreeNavigation, FolhaNaoDesce)
{
   const app::ComponentTreeLayout layout{app::layoutComponentTree(makeSampleTree(), {})};
   app::ComponentTreeViewState view;
   app::CollapsedNodes collapsed;
   view.selectedKey = "/plugins";

   EXPECT_FALSE(app::navigateComponentTree(layout, view, app::TreeNavigation::FirstChild, collapsed));
   EXPECT_EQ(view.selectedKey, "/plugins");
}

//------------------------------------------------------------------------------
// Enquadramento.
//------------------------------------------------------------------------------
TEST(ComponentTreeFit, NuncaAmpliaAlemDeCemPorCento)
{
   // Arvore minuscula num canvas grande: sem o teto, o enquadramento
   // ampliaria (medido: 180%) e o primeiro galho expandido ja estouraria.
   app::ComponentTreeNode root;
   root.nodeKey = "/";
   root.className = "Station";
   root.children.push_back(makeNode("/", "a"));

   app::ComponentTreeViewState view;
   view.canvasWidthPx = 2000;
   view.canvasHeightPx = 1000;

   app::fitComponentTreeToContent(view, app::layoutComponentTree(root, {}));
   EXPECT_LE(view.zoom, 1.0);
}

TEST(ComponentTreeFit, ArvoreLargaEncolheParaCaber)
{
   app::ComponentTreeNode root;
   root.nodeKey = "/";
   root.className = "Station";
   for (int i = 0; i < 40; i++) {
      root.children.push_back(makeNode("/", "filho_com_nome_longo_" + std::to_string(i)));
   }

   app::ComponentTreeViewState view;
   view.canvasWidthPx = 240;
   view.canvasHeightPx = 120;

   app::fitComponentTreeToContent(view, app::layoutComponentTree(root, {}));
   EXPECT_LT(view.zoom, 1.0);
   EXPECT_GE(view.zoom, app::kTreeMinZoom);
}
