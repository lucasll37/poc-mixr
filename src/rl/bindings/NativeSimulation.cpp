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
   // O comando pendente e invalidado antes do frame de priming que
   // primeStation() dispara (RESET_EVENT + tcFrame() de aquecimento, que ja
   // chama RLBridgeBehavior::genAction()) -- senao o ultimo comando do
   // episodio anterior vazaria para o priming do proximo, ou, no primeiro
   // reset() do processo, um Command{} default aplicaria
   // heading=0/altitude=0/speed=0 como se fosse uma decisao de verdade.
   mixr::xrlbridge::setPendingCommand(mixr::xrlbridge::Command{});

   if (!built_) {
      station_ = buildStation(scenarioPath_);
      primeStation(station_);

      // Sem esta checagem, um 'playerName_' com typo (ou apontando para um
      // player que existe mas nao e o configurado com '( RLBridgeBehavior )'
      // no .edl) faz step() rodar indefinidamente com 'terminated' preso em
      // false: findPlayerByName() devolve nullptr, mas Command/Observation
      // continuam fluindo normalmente pelo unico agente RL do processo. A
      // checagem so confirma a existencia do player -- nao e possivel
      // confirmar daqui que e o mesmo player com RLBridgeBehavior, tipo que
      // mora no plugin do modelo e este core nao pode conhecer
      // (tests/guard/check_core_opaco.sh).
      const auto player = worldModelOf(station_)->findPlayerByName(playerName_.c_str());
      if (player == nullptr) {
         throw std::runtime_error(
            "NativeSimulation: player '" + playerName_ + "' nao existe no cenario '"
            + scenarioPath_ + "'. player_name tem de ser o MESMO player configurado "
            "com ( RLBridgeBehavior ) no .edl (default: falcon1) -- ver src/rl/README.md.");
      }

      built_ = true;
   } else {
      // reset() repetido na mesma Station, via RESET_EVENT, e seguro: produz
      // apenas uma pequena deriva numerica de integracao, nao um erro de
      // reset (ver o cabecalho .hpp).
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
