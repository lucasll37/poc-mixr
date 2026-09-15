#include "bt/nodes/NavigateAction.hpp"

#include "bt/DecisionContext.hpp"
#include "domain/geometry.hpp"

#include <algorithm>

namespace mixr {
namespace models {
namespace xA_4 {
namespace bt_nodes {

namespace {

// Taxa maxima do rumo comandado, deliberadamente mais apertada que o
// maxRateOfTurnDps do Autopilot: o limite fisico da aeronave nao evita a
// divergencia de perseguicao pura (observada girando bem abaixo do teto de
// guinada) -- o limite precisa estar no comando, nao na planta.
constexpr double kMaxHeadingRateDegPerSec{3.0};

} // namespace

NavigateAction::NavigateAction(const std::string& name, const BT::NodeConfiguration& config,
                               const NodeContext& context)
   : BT::SyncActionNode(name, config), context_(context)
{
}

//------------------------------------------------------------------------------
// tick() -- traduz a guiagem que Route/Steerpoint ja calcularam em um
// FlightCommand.
//
// getTrueBrgDeg() e a marcacao direto-ao-ponto recalculada a cada frame;
// comanda-la crua como rumo e perseguicao pura (pure pursuit) sem termo de
// avanco, que diverge quando o angulo de marcacao muda mais rapido que a
// taxa de guinada disponivel -- tipicamente perto do ponto de destino. Sem
// limitador de taxa, a aeronave perde velocidade/altitude continuamente ate
// entrar em espiral e colidir com o terreno (falha silenciosa de fisica,
// sem erro de parse). O limitador abaixo amortece o comando para se
// aproximar suavemente, com avanco implicito; Route/Steerpoint continuam
// sendo a unica fonte do alvo.
//
// Sem fallback de proposito, igual antes: se a rota nao tiver guiagem
// valida, o no falha e nao ha decisao naquele tick.
//------------------------------------------------------------------------------
BT::NodeStatus NavigateAction::tick()
{
   if (context_.behavior == nullptr) return BT::NodeStatus::FAILURE;

   const auto& view = context_.behavior->snapshot();
   if (!view.hasNavSteering) {
      // Sem resetar hasCommandedHeading_, um gap de guiagem invalida (ex.:
      // a arvore troca para EVADE e volta para NAV) deixaria
      // commandedHeadingDeg_ congelado no ultimo valor antes do gap; o
      // proximo tick valido cairia no ramo de suavizacao, tentando corrigir
      // de um rumo antigo para um possivelmente muito diferente pela taxa
      // limitada -- reintroduzindo a mesma instabilidade que o limitador
      // existe para evitar, agora na reconexao.
      hasCommandedHeading_ = false;
      return BT::NodeStatus::FAILURE;
   }

   if (!hasCommandedHeading_) {
      // Primeiro tick com guiagem valida: nao ha rumo anterior para
      // suavizar a partir dele -- comeca exatamente na marcacao.
      commandedHeadingDeg_ = view.navTrueBrgDeg;
      hasCommandedHeading_ = true;
   } else {
      const double dt{context_.behavior->getFrameDt()};
      const double errorDeg{domain::wrap180(view.navTrueBrgDeg - commandedHeadingDeg_)};
      const double maxStepDeg{kMaxHeadingRateDegPerSec * dt};
      const double stepDeg{std::clamp(errorDeg, -maxStepDeg, maxStepDeg)};

      commandedHeadingDeg_ = domain::wrap360(commandedHeadingDeg_ + stepDeg);
   }

   domain::FlightCommand cmd;
   cmd.headingDeg = commandedHeadingDeg_;
   // A altitude do Steerpoint e um valor estatico do .edl, sem visibilidade
   // do banco de elevacao real. Sem o mesmo piso anti-CFIT que Patrol/RTB/
   // Support/SlowRoll ja aplicam, uma rota cujo relevo real e mais alto que
   // o assumido pelo autor faz o Navigate manter voo reto e nivelado direto
   // para dentro do terreno -- situacao observada apos um giro que reduz
   // altitude e a arvore retorna a NAV abaixo do necessario.
   cmd.altitudeM = context_.behavior->clampAltitudeToTerrain(
      view.hasNavCmdAlt ? view.navCmdAltM : view.altitudeM);
   cmd.speedKts = view.hasNavCmdSpeed ? view.navCmdSpeedKts : view.speedKts;

   context_.behavior->decision().take(cmd, "NAV");
   return BT::NodeStatus::SUCCESS;
}

} // namespace bt_nodes
} // namespace xA_4
} // namespace models
} // namespace mixr
