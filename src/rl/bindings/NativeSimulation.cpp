#include "NativeSimulation.hpp"

#include "StationBuilder.hpp"

#include "mixr/simulation/Station.hpp"
#include "mixr/models/WorldModel.hpp"
#include "mixr/simulation/AbstractPlayer.hpp"

#include "mixr/base/Component.hpp"

#include <stdexcept>

namespace rl {

NativeSimulation::NativeSimulation(std::string scenarioPath, std::string playerName)
   : scenarioPath_(std::move(scenarioPath)), playerName_(std::move(playerName))
{
}

NativeSimulation::~NativeSimulation()
{
   // Nunca dlclose() do plugin do modelo (ver a mesma regra documentada no
   // CLAUDE.md raiz, secao do SDK de plugin) -- o processo Python que
   // embute este modulo termina e o SO recupera tudo; nada a fazer aqui
   // alem de deixar o Station* vazar com o processo, mesmo raciocinio do
   // resto das pocs deste repositorio (nenhuma delas chama
   // station->unref() no fim de main()).
}

mixr::xrlbridge::Observation NativeSimulation::reset()
{
   // ACHADO POR AUDITORIA (nao redescobrir, ver o comentario grande em
   // xrlbridge::Command): invalida o comando pendente ANTES do frame de
   // priming que primeStation() dispara (RESET_EVENT + tcFrame() de
   // aquecimento, que ja chama RLBridgeBehavior::genAction() antes de
   // qualquer step()/setPendingCommand() deste episodio) -- sem isto, o
   // ULTIMO comando do episodio ANTERIOR vazava pro priming do proximo (ou,
   // no primeiro reset() do processo, um Command{} default aplicava
   // heading=0/altitude=0/speed=0 como se fosse uma decisao de verdade).
   mixr::xrlbridge::setPendingCommand(mixr::xrlbridge::Command{});

   if (!built_) {
      station_ = buildStation(scenarioPath_);
      primeStation(station_);

      // BUG CONFIRMADO E CORRIGIDO AQUI -- sem esta checagem, um 'playerName_'
      // com typo (ou apontando pra um player que existe mas nao e o mesmo
      // configurado com '( RLBridgeBehavior )' no .edl -- ex.: falcon2..4
      // neste cenario) fazia step() rodar para sempre com 'terminated' preso
      // em false, silenciosamente: findPlayerByName() abaixo devolvia nullptr
      // (nome inexistente) e o Command/Observation continuavam fluindo
      // normalmente pelo UNICO agente RL do processo (falcon1, fixo no
      // .edl) -- ninguem percebia que o 'player_name' pedido nao tinha nada
      // a ver com o que estava de fato sendo controlado/observado. Falha
      // rapido e alto em vez de treinar contra um sinal errado.
      //
      // So confere EXISTENCIA do player -- nao da para confirmar daqui que e
      // o MESMO player com RLBridgeBehavior: esse tipo mora no plugin do
      // modelo (models/players/A-4), que este host nao pode conhecer
      // (tests/guard/check_host_opaco.sh). 'player_name' tem de bater com o
      // player que o .edl configurou com RLBridgeBehavior -- ver
      // src/rl/README.md.
      const auto player = worldModelOf(station_)->findPlayerByName(playerName_.c_str());
      if (player == nullptr) {
         throw std::runtime_error(
            "NativeSimulation: player '" + playerName_ + "' nao existe no cenario '"
            + scenarioPath_ + "'. player_name tem de ser o MESMO player configurado "
            "com ( RLBridgeBehavior ) no .edl (default: falcon1) -- ver src/rl/README.md.");
      }

      built_ = true;
   } else {
      // reset() repetido na MESMA Station, via RESET_EVENT -- CONFIRMADO
      // seguro (ver o cabecalho .hpp): pequena deriva numerica de
      // integracao, nao um erro de reset.
      primeStation(station_);
   }

   return mixr::xrlbridge::getObservation();
}

std::pair<mixr::xrlbridge::Observation, bool> NativeSimulation::step(
   const mixr::xrlbridge::Command& cmd)
{
   if (!built_) {
      throw std::runtime_error("NativeSimulation::step() chamado antes de reset()");
   }

   mixr::xrlbridge::setPendingCommand(cmd);

   // Mesma dupla chamada de app::runDeterministic()/app::DeterministicRun.cpp
   // -- tcFrame() dispara a fase 3 (FlightAgentTC -> RLBridgeBehavior::
   // genAction(), que publica a Observation deste frame e consome o Command
   // que acabou de ser publicado acima), updateData() drena o gravador.
   const double dt{1.0 / static_cast<double>(station_->getTimeCriticalRate())};
   station_->tcFrame(dt);
   station_->updateData(dt);

   const auto player = worldModelOf(station_)->findPlayerByName(playerName_.c_str());
   const bool terminated = (player != nullptr) && player->isCrashed();

   return {mixr::xrlbridge::getObservation(), terminated};
}

} // namespace rl
