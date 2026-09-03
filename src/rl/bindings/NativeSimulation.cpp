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
   if (!built_) {
      station_ = buildStation(scenarioPath_);
      primeStation(station_);
      built_ = true;
   } else {
      // RISCO A VERIFICAR (ver o cabecalho .hpp) -- reset de cenario via
      // RESET_EVENT, no MESMO processo, sem reconstruir a Station.
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

   const auto worldModel = dynamic_cast<mixr::models::WorldModel*>(station_->getSimulation());
   bool terminated{};
   if (worldModel != nullptr) {
      const auto player = worldModel->findPlayerByName(playerName_.c_str());
      terminated = (player != nullptr) && player->isCrashed();
   }

   return {mixr::xrlbridge::getObservation(), terminated};
}

} // namespace rl
