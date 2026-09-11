#pragma once

#include "bt/NodeContext.hpp"
#include "domain/AerobaticPlan.hpp"
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
// Antes, NodeContext carregava um mixr::models::xnative::BtBehavior* cru. Os headers
// dos nos ja eram limpos, mas todo .cpp tinha de incluir "ubf/BtBehavior.hpp"
// para chamar oito getters -- e com ele vinha o MIXR inteiro. O efeito
// pratico era que a arvore, a peca mais propria desta poc, so podia ser
// exercitada subindo uma Station.
//
// Esta interface e aquele conjunto de getters (hoje nove). BtBehavior a
// implementa sem escrever um metodo novo: as assinaturas ja eram estas
// (exceto clampAltitudeToTerrain(), acrescentado para fechar um buraco
// achado por auditoria -- ver o comentario dela mais abaixo).
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

   // Quando fazer a proxima acrobacia, e por quanto tempo mante-la. Mesmo
   // contrato dos planos acima: o estado sobrevive entre ticks, e quem o
   // configura/semeia e' BtBehavior::configurePlans() -- o no
   // ( SlowRoll ) so o avanca e le.
   virtual domain::AerobaticPlan& aerobaticPlan() = 0;

   // SEGUNDA instancia de domain::ThreatPolicy, dedicada a ameaca de RWR
   // (emissor hostil detectado passivamente, ver domain/WorldView.hpp) --
   // independente de threatPolicy() acima (contato de radar proprio contra
   // outra aeronave). As duas nunca interferem entre si: ThreatPolicy
   // recebe TODO o estado por parametro a cada update(), sem nenhum membro
   // compartilhado. Alimentada em ubf::BtBehavior::feedRwrEvasion(), so'
   // com hasContact=true depois que domain::EvasionReactionPlan libera a
   // manobra (o atraso estocastico de reacao do piloto).
   virtual const domain::ThreatPolicy& rwrThreatPolicy() const = 0;

   // Alcance/cone dentro dos quais o disparo de missil e' permitido (ver
   // domain/LaunchPolicy.hpp e bt/nodes/LaunchEnvelopeCondition.cpp). Ao
   // contrario dos planos acima, nao ha estado que sobreviva entre ticks
   // aqui -- e so' os numeros configurados pelos slots
   // launchMinRange/launchMaxRange/launchCone (ver ubf/BtBehavior.hpp).
   virtual const domain::LaunchEnvelope& launchEnvelope() const = 0;

   // parametros do ciclo e dos slots do EDL
   virtual double getFrameDt() const = 0;
   virtual double getFuelReserve() const = 0;
   virtual double getSupportSpeedKts() const = 0;

   // O piso anti-CFIT (domain/TerrainFloor.hpp) -- ACHADO POR AUDITORIA
   // (nao redescobrir): so domain::ThreatPolicy::breakCommand() aplicava
   // este piso; RTB e SUPPORT comandavam altitude (rtbAltitude fixo do
   // EDL, ou a altitude ABSOLUTA de um contato reportado por outro player)
   // sem NENHUMA validacao contra o terreno em runtime. Com o cenario de
   // producao "SEM ARBITRO" (nenhum AltitudeSafetyBehavior por cima),
   // ThreatPolicy tinha virado a UNICA camada de protecao ativa -- os
   // outros ramos so estavam seguros porque o rtbAltitude de cada falcon
   // foi calibrado a mao contra o pico do PROPRIO circuito, nao contra o
   // caminho de volta de verdade. Cada no que comanda altitude fora do
   // ramo de evasao deve passar por aqui antes de decision().take() --
   // ver ReturnToBaseAction/SupportAlertAction/PatrolAction.
   virtual double clampAltitudeToTerrain(double altitudeM) const = 0;

   // Ha altitude de sobra para COMECAR uma acrobacia (domain::AerobaticPlan)
   // agora? -- ACHADO investigando uma colisao real: um giro de aileron
   // puro, sem compensacao de profundor, custa bem mais que a media
   // documentada quando o banco passa por invertido (o altitude hold
   // comanda profundor no sentido CONTRARIO nessa faixa -- ver o README de
   // sandbox/A4-6DOF-RANDOM). Sem esta borda, giros sucessivos catam
   // altitude, um perto do pior caso apos o outro, ate colidir com o
   // terreno -- medido rodando: as 8 aeronaves daquele cenario colidem entre
   // t=680s e t=1142s sem este piso, nenhuma com ele em 4000s (ver
   // ubf/BtTuning.hpp::slowRollMinMarginM). So' a BORDA Idle->Rolling de
   // AerobaticPlan::update() consulta isto -- uma vez em curso, a manobra
   // sempre completa os 360 graus ou estoura por timeout, nunca aborta no
   // meio (ver o comentario de AerobaticPlan::update()).
   virtual bool hasAerobaticAltitudeMargin() const = 0;
};

} // namespace bt_nodes
