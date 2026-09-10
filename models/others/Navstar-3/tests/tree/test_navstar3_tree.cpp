//
// A ARVORE DE PRODUCAO, carregada de verdade, SEM levantar Station nenhuma.
//
// E' a camada que so' existe porque bt/DecisionContext.hpp e' uma interface:
// os nos nunca tocam um objeto MIXR, entao um contexto FALSO (abaixo) basta
// para exercitar a maquina de estados inteira. 'ldd' neste binario nao mostra
// nenhuma lib do MIXR -- se algum dia mostrar, alguem incluiu um header do
// framework em bt/ e a propriedade se perdeu.
//
// O caminho do .xml chega por -DNAVSTAR3_TREE_XML (ver tests/meson.build): e'
// o arquivo REAL de configs/, nunca uma copia -- uma copia comecaria certa e
// envelheceria em silencio.
//
#include "bt/DecisionContext.hpp"
#include "bt/bt_factory.hpp"

#include <gtest/gtest.h>

#include <string>

namespace {

using namespace mixr::models::xNavstar_3;

// O contexto falso: guarda o que os nos leem e o que eles escrevem, e nada
// mais.
class FakeContext final : public bt::DecisionContext
{
public:
   domain::SunState sunState() const override   { return sunState_; }
   bt::Navstar3Decision& decision() override    { return decision_; }

   domain::SunState sunState_{domain::SunState::SUNLIT};
   bt::Navstar3Decision decision_{};
};

// Monta a MESMA factory que ubf::Navstar3BtBehavior monta e carrega a MESMA
// arvore que o .so instala.
BT::Tree carregarArvore(FakeContext& ctx, BT::BehaviorTreeFactory& factory)
{
   bt::NodeContext node;
   node.behavior = &ctx;
   bt::registerNodes(factory, node);
   return factory.createTreeFromFile(NAVSTAR3_TREE_XML, BT::Blackboard::create());
}

std::string decidir(FakeContext& ctx, BT::Tree& arvore)
{
   ctx.decision_.reset();
   arvore.tickRoot();
   return ctx.decision_.label;
}

TEST(Navstar3Tree, CarregaSemNoDesconhecido)
{
   FakeContext ctx;
   BT::BehaviorTreeFactory factory;
   EXPECT_NO_THROW({ BT::Tree t = carregarArvore(ctx, factory); });
}

TEST(Navstar3Tree, NaSombraDaTerraDecideEclipse)
{
   FakeContext ctx;
   ctx.sunState_ = domain::SunState::ECLIPSE;

   BT::BehaviorTreeFactory factory;
   BT::Tree arvore{carregarArvore(ctx, factory)};

   EXPECT_EQ("ECLIPSE", decidir(ctx, arvore));
}

TEST(Navstar3Tree, ForaDaSombraDecideSunlit)
{
   FakeContext ctx;
   ctx.sunState_ = domain::SunState::SUNLIT;

   BT::BehaviorTreeFactory factory;
   BT::Tree arvore{carregarArvore(ctx, factory)};

   EXPECT_EQ("SUNLIT", decidir(ctx, arvore));
}

// A arvore sempre decide alguma coisa: um Fallback cujo ultimo ramo fosse
// condicional devolveria FAILURE e deixaria o satelite sem decisao no frame.
TEST(Navstar3Tree, TodoCicloProduzUmaDecisao)
{
   FakeContext ctx;
   BT::BehaviorTreeFactory factory;
   BT::Tree arvore{carregarArvore(ctx, factory)};

   for (const domain::SunState s : {domain::SunState::SUNLIT, domain::SunState::ECLIPSE}) {
      ctx.sunState_ = s;
      ctx.decision_.reset();
      const BT::NodeStatus status{arvore.tickRoot()};
      EXPECT_EQ(BT::NodeStatus::SUCCESS, status);
      EXPECT_TRUE(ctx.decision_.taken);
   }
}

} // namespace
