#include "bt/nodes/NavigateAction.hpp"

#include "bt/DecisionContext.hpp"
#include "domain/geometry.hpp"

#include <algorithm>

namespace bt_nodes {

namespace {

// Taxa maxima do RUMO COMANDADO, deliberadamente mais apertada que o
// maxRateOfTurnDps do Autopilot (6 deg/s nos cenarios desta poc): o limite
// da AERONAVE nao e o que causa o problema (medido rodando: a divergencia
// aparecia girando a ~1.6 deg/s, bem abaixo do teto da aeronave) -- o
// limite tem de estar no COMANDO em si. Ver o comentario de tick().
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
// RECALCULADA A CADA FRAME a partir da posicao atual (Steerpoint::compute(),
// Route.cpp/Steerpoint.cpp) -- e o MESMO dado que
// Autopilot::processModeNavigation() consultaria com navMode ligado
// (Navigation::updateNavSteering() faz exatamente
// setTrueBrgDeg(to->getTrueBrgDeg())). Comandar ISSO direto como rumo, a
// cada tick, e perseguicao pura (pure pursuit) sem termo de avanco -- e
// diverge perto do alvo pelo MESMO motivo ja documentado na secao "Demo:
// missil guiado" deste repositorio para domain::pursuit(): um controlador
// SO proporcional (aqui, rumo = marcacao, ganho infinito/instantaneo via
// headingHoldMode) fica instavel quando o angulo de marcacao passa a mudar
// rapido demais para a taxa de guinada disponivel -- perto de um ponto
// PARADO, isso acontece justamente quando o avantajado se aproxima, ainda
// que dentro do limite fisico de guinada da aeronave (medido rodando SEM
// o limitador abaixo: velocidade e altitude caindo continuamente por
// ~200 s simulados ate a aeronave entrar em espiral e colidir com o
// terreno -- nenhum erro de parse, nenhum "was not found", so a fisica
// da perseguicao pura degradando).
//
// O limitador de taxa abaixo NAO e uma regra de negocio nova: e a MESMA
// tecnica que qualquer guiador por perseguicao pura precisa -- rumo
// comandado nunca pula mais que kMaxHeadingRateDegPerSec por segundo,
// convertendo o comando bruto de marcacao instantanea num comando que se
// aproxima suavemente, com avanco implicito. Route/Steerpoint continuam
// sendo a UNICA fonte do alvo (marcacao, altitude, velocidade); nada aqui
// reimplementa navegacao -- so amortece o COMANDO.
//
// Sem fallback de proposito, igual antes: se a rota nao tiver guiagem
// valida, o no falha e nao ha decisao naquele tick.
//------------------------------------------------------------------------------
BT::NodeStatus NavigateAction::tick()
{
   if (context_.behavior == nullptr) return BT::NodeStatus::FAILURE;

   const auto& view = context_.behavior->snapshot();
   if (!view.hasNavSteering) return BT::NodeStatus::FAILURE;

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

} // namespace bt_nodes
