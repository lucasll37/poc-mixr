#pragma once

#include "bt/NodeContext.hpp"
#include "domain/LaunchPolicy.hpp"
#include "domain/PatrolPlan.hpp"
#include "domain/RtbPlan.hpp"
#include "domain/ThreatPolicy.hpp"
#include "domain/WorldView.hpp"

namespace bt_nodes {

//------------------------------------------------------------------------------
// DecisionContext -- o que um no da arvore precisa do comportamento que o
// hospeda, e NADA ALEM DISSO.
//
// Antes, NodeContext carregava um mixr::xnative::BtBehavior* cru. Os headers
// dos nos ja eram limpos, mas todo .cpp tinha de incluir "ubf/BtBehavior.hpp"
// para chamar oito getters -- e com ele vinha o MIXR inteiro. O efeito
// pratico era que a arvore, a peca mais propria desta poc, so podia ser
// exercitada subindo uma Station.
//
// Esta interface e exatamente aquele conjunto de oito getters. BtBehavior a
// implementa sem escrever um metodo novo: as assinaturas ja eram estas.
//
// O que isso compra: bt/nodes/*.cpp e bt/bt_factory.cpp passam a compilar
// contra BehaviorTree.CPP + domain/ apenas. Um teste monta um
// FakeDecisionContext, carrega o flight_tree.xml DE PRODUCAO e verifica qual
// ramo venceu -- sem simulacao, sem player, sem terreno.
//
// (O comentario de NodeContext ja prometia "ou a um teste unitario sem
// simulacao nenhuma"; faltava o tipo abstrato para cumprir a promessa.)
//------------------------------------------------------------------------------
class DecisionContext
{
public:
   virtual ~DecisionContext() = default;

   // percepcao do frame
   virtual const domain::WorldView& snapshot() const = 0;

   // o que a arvore preenche neste tick
   virtual FlightDecision& decision() = 0;

   // planos de voo, com o estado que sobrevive entre ticks
   virtual domain::PatrolPlan& patrolPlan() = 0;
   virtual domain::RtbPlan& rtbPlan() = 0;
   virtual const domain::ThreatPolicy& threatPolicy() const = 0;

   // parametros do ciclo e dos slots do EDL
   virtual double getFrameDt() const = 0;
   virtual double getFuelReserve() const = 0;
   virtual double getSupportSpeedKts() const = 0;

   // envelope de lancamento do missil (ver domain/LaunchPolicy.hpp) --
   // so usado por LaunchEnvelopeCondition; nos demais avioes (sem 'stores:')
   // WorldView::weaponReady ja falha antes de este valor importar.
   virtual const domain::LaunchEnvelope& launchEnvelope() const = 0;
};

} // namespace bt_nodes
