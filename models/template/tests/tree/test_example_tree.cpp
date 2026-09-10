//
// A ARVORE DE PRODUCAO, carregada de verdade, SEM levantar Station nenhuma.
//
// E' a camada que so' existe porque bt/DecisionContext.hpp e' uma interface:
// os nos nunca tocam um objeto MIXR, entao um contexto FALSO (abaixo) basta
// para exercitar a maquina de estados inteira. 'ldd' neste binario nao mostra
// nenhuma lib do MIXR -- se algum dia mostrar, alguem incluiu um header do
// framework em bt/ e a propriedade se perdeu.
//
// O caminho do .xml chega por -DEXAMPLE_TREE_XML (ver tests/meson.build): e'
// o arquivo REAL de configs/, nunca uma copia -- uma copia comecaria certa e
// envelheceria em silencio.
//
#include "bt/DecisionContext.hpp"
#include "bt/bt_factory.hpp"

#include <gtest/gtest.h>

#include <string>

namespace {

using namespace mixr::models::xtemplate;

// O contexto falso: guarda o que os nos leem e o que eles escrevem, e nada
// mais. Um teste de modelo de verdade normalmente cresce daqui (ver
// FakeDecisionContext em models/players/A-4/tests/tree/).
class FakeContext final : public bt::DecisionContext
{
public:
   double value() const override                          { return value_; }
   const domain::ExampleThreshold& rule() const override  { return rule_; }
   bool engaged() const override                          { return engaged_; }
   void setEngaged(const bool v) override                 { engaged_ = v; }
   bt::ExampleDecision& decision() override               { return decision_; }

   // Sufixo '_' porque os cinco nomes nus ja sao os metodos virtuais acima.
   double value_{};
   domain::ExampleThreshold rule_{};
   bool engaged_{false};
   bt::ExampleDecision decision_{};
};

// Monta a MESMA factory que ubf::ExampleBehavior monta e carrega a MESMA
// arvore que o .so instala.
BT::Tree carregarArvore(FakeContext& ctx, BT::BehaviorTreeFactory& factory)
{
   bt::NodeContext node;
   node.behavior = &ctx;
   bt::registerNodes(factory, node);
   return factory.createTreeFromFile(EXAMPLE_TREE_XML, BT::Blackboard::create());
}

std::string decidir(FakeContext& ctx, BT::Tree& arvore)
{
   ctx.decision_.reset();
   arvore.tickRoot();
   return ctx.decision_.label;
}

TEST(ExampleTree, CarregaSemNoDesconhecido)
{
   FakeContext ctx;
   BT::BehaviorTreeFactory factory;
   EXPECT_NO_THROW({ BT::Tree t = carregarArvore(ctx, factory); });
}

TEST(ExampleTree, AbaixoDoLimiarCaiNoRamoDeDegradacao)
{
   FakeContext ctx;
   ctx.rule_.onValue = 10.0;
   ctx.rule_.offValue = 5.0;
   ctx.value_ = 1.0;

   BT::BehaviorTreeFactory factory;
   BT::Tree arvore{carregarArvore(ctx, factory)};

   EXPECT_EQ("IDLE", decidir(ctx, arvore));
   EXPECT_FALSE(ctx.engaged_);
}

TEST(ExampleTree, AcimaDoLimiarEngajaEVenceOPrimeiroRamo)
{
   FakeContext ctx;
   ctx.rule_.onValue = 10.0;
   ctx.rule_.offValue = 5.0;
   ctx.value_ = 12.0;

   BT::BehaviorTreeFactory factory;
   BT::Tree arvore{carregarArvore(ctx, factory)};

   EXPECT_EQ("ENGAGED", decidir(ctx, arvore));
   EXPECT_TRUE(ctx.engaged_);
}

// A propriedade que justifica a regra existir: entre offValue e onValue o
// resultado depende do estado ANTERIOR, nao so' da leitura. Sem histerese
// os dois casos abaixo dariam a MESMA resposta.
TEST(ExampleTree, NaFaixaDeHistereseAArvorePreservaOEstadoAnterior)
{
   FakeContext ctx;
   ctx.rule_.onValue = 10.0;
   ctx.rule_.offValue = 5.0;

   BT::BehaviorTreeFactory factory;
   BT::Tree arvore{carregarArvore(ctx, factory)};

   ctx.value_ = 12.0;
   EXPECT_EQ("ENGAGED", decidir(ctx, arvore));

   ctx.value_ = 7.0;   // caiu, mas ainda nao abaixo de offValue
   EXPECT_EQ("ENGAGED", decidir(ctx, arvore));

   ctx.value_ = 4.0;   // agora sim
   EXPECT_EQ("IDLE", decidir(ctx, arvore));

   ctx.value_ = 7.0;   // subiu, mas ainda nao ate onValue
   EXPECT_EQ("IDLE", decidir(ctx, arvore));
}

// A arvore sempre decide alguma coisa: um Fallback cujo ultimo ramo fosse
// condicional devolveria FAILURE e deixaria a entidade sem decisao no frame.
TEST(ExampleTree, TodaLeituraProduzUmaDecisao)
{
   FakeContext ctx;
   ctx.rule_.onValue = 10.0;
   ctx.rule_.offValue = 5.0;

   BT::BehaviorTreeFactory factory;
   BT::Tree arvore{carregarArvore(ctx, factory)};

   for (const double v : {-100.0, 0.0, 4.9, 5.0, 9.9, 10.0, 1e6}) {
      ctx.value_ = v;
      ctx.decision_.reset();
      const BT::NodeStatus status{arvore.tickRoot()};
      EXPECT_EQ(BT::NodeStatus::SUCCESS, status) << "valor " << v;
      EXPECT_TRUE(ctx.decision_.taken) << "valor " << v;
   }
}

} // namespace
