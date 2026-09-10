#include "bt/nodes/NavigateAction.hpp"

#include "bt/DecisionContext.hpp"
#include "domain/geometry.hpp"

#include <algorithm>

namespace mixr {
namespace models {
namespace xC_130 {
namespace bt {

namespace {

// Taxa maxima do RUMO COMANDADO -- NAO CALIBRADA para o C-130, herdada como
// ponto de partida do valor medido em models/players/A-4 (3.0 deg/s, contra um
// maxRateOfTurnDps de 6 deg/s no Autopilot daquele modelo). O C-130 e uma
// aeronave bem maior e mais lenta para guinar; este numero provavelmente
// precisa ficar MENOR -- medir rodando (ver README.md desta poc) antes de
// confiar nele. Sem o limitador: perseguicao pura (rumo = marcacao
// instantanea, ganho infinito via headingHoldMode) diverge perto do alvo
// pelo mesmo motivo documentado no NavigateAction da A-4 -- ver o cabecalho
// de tick() abaixo.
constexpr double kMaxHeadingRateDegPerSec{3.0};

} // namespace

NavigateAction::NavigateAction(const std::string& name, const BT::NodeConfiguration& config,
                               const NodeContext& context)
   : BT::SyncActionNode(name, config), context_(context)
{
}

//------------------------------------------------------------------------------
// tick() -- traduz a guiagem que o Route/Steerpoint NATIVO ja calculou (ver
// FlightState::updateState()) num FlightCommand.
//
// mixr::models::Navigation::getTrueBrgDeg() e a marcacao "direto-ao-ponto"
// RECALCULADA A CADA FRAME a partir da posicao atual -- e o MESMO dado que
// Autopilot::processModeNavigation() consultaria com navMode ligado.
// Comandar ISSO direto como rumo, a cada tick, e perseguicao pura (pure
// pursuit) sem termo de avanco -- diverge perto do ponto pelo mesmo motivo
// ja documentado em models/players/A-4: um controlador SO proporcional fica
// instavel quando o angulo de marcacao muda mais rapido que a taxa de
// guinada disponivel.
//
// O limitador de taxa abaixo NAO e uma regra de negocio nova: converte o
// comando bruto de marcacao instantanea num comando que se aproxima
// suavemente, com avanco implicito. Route/Steerpoint continuam sendo a
// UNICA fonte do alvo (marcacao, altitude, velocidade); nada aqui
// reimplementa navegacao -- so amortece o COMANDO.
//
// Sem fallback de proposito: se a rota nao tiver guiagem valida, o no falha
// e nao ha decisao naquele tick.
//------------------------------------------------------------------------------
BT::NodeStatus NavigateAction::tick()
{
   if (context_.behavior == nullptr) return BT::NodeStatus::FAILURE;

   const auto& view = context_.behavior->snapshot();
   if (!view.hasNavSteering) {
      // Sem isto, um GAP de guiagem invalida deixaria 'commandedHeadingDeg_'
      // CONGELADO no ultimo valor de antes do gap -- o proximo tick com
      // guiagem valida cairia no ramo de SUAVIZACAO (nao no de "primeiro
      // tick"), tentando corrigir de um rumo antigo pra um possivelmente
      // muito diferente pela taxa limitada. Ver o mesmo achado por auditoria
      // em models/players/A-4/src/bt/nodes/NavigateAction.cpp.
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
   cmd.altitudeM = view.hasNavCmdAlt ? view.navCmdAltM : view.altitudeM;
   cmd.speedKts = view.hasNavCmdSpeed ? view.navCmdSpeedKts : view.speedKts;

   context_.behavior->decision().take(cmd, "NAV");
   return BT::NodeStatus::SUCCESS;
}

} // namespace bt
} // namespace xC_130
} // namespace models
} // namespace mixr
