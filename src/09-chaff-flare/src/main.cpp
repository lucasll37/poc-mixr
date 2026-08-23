//
// poc/09-chaff-flare
//
// Mesmas features das pocs anteriores (6-DOF via JSBSimModel/F4N,
// Tacview) -- agora com lancamento periodico de chaff e flare. Sao os
// efeitos NATIVOS do MIXR (mixr::models::Chaff/Flare, subclasses de
// Effect/AbstractWeapon -- "podem ser released e virar players
// independentes", mesmo mecanismo de qualquer arma), guardados num
// StoresMgr comum (ver scenario.epp) e liberados via
// StoresMgr::releaseOneChaff()/releaseOneFlare() -- nao ha nenhuma
// fisica/timer de efeito escrito por nos: a queda/deriva do chaff/flare
// e a propria dinamica nativa do Effect, e o seu desaparecimento
// automatico apos 10s (Effect::maxTOF, default nativo) e so espelhado
// aqui pra tambem remover o objeto no Tacview (linha ACMI "-<id>").
//
// Tags ACMI usadas (conferidas contra a documentacao oficial do
// Tacview): aeronave "Air+FixedWing"; chaff "Misc+Decoy+Chaff"; flare
// "Misc+Decoy+Flare" -- sao essas tags que fazem o Tacview desenhar o
// icone/particula correta de cada tipo.
//

#include "mixr_factory.hpp"

#include "mixr/simulation/Station.hpp"
#include "mixr/simulation/AbstractPlayer.hpp"
#include "mixr/models/player/Player.hpp"
#include "mixr/models/dynamics/DynamicsModel.hpp"
#include "mixr/models/system/StoresMgr.hpp"
#include "mixr/models/player/effect/Chaff.hpp"
#include "mixr/models/player/effect/Flare.hpp"
#include "mixr/base/edl_parser.hpp"
#include "mixr/base/Pair.hpp"
#include "mixr/base/util/system_utils.hpp"

#include <algorithm>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

const int bgRate{10};
const double releaseIntervalSec{15.0};  // libera um par chaff+flare a cada 15s
const double effectMaxTofSec{10.0};     // mesmo Effect::maxTOF nativo (default)

volatile std::sig_atomic_t g_stopRequested{0};
void onSigint(int) { g_stopRequested = 1; }

mixr::simulation::Station* buildStation(const std::string& filename)
{
   int num_errors{};
   mixr::base::Object* obj{mixr::base::edl_parser(filename, mixrFactory, &num_errors)};
   if (num_errors > 0) {
      std::cerr << "File: " << filename << ", number of errors: " << num_errors << std::endl;
      std::exit(EXIT_FAILURE);
   }
   if (obj == nullptr) {
      std::cerr << "Invalid configuration file, no objects defined!" << std::endl;
      std::exit(EXIT_FAILURE);
   }

   const auto pair = dynamic_cast<mixr::base::Pair*>(obj);
   if (pair != nullptr) {
      obj = pair->object();
      obj->ref();
      pair->unref();
   }

   const auto station = dynamic_cast<mixr::simulation::Station*>(obj);
   if (station == nullptr) {
      std::cerr << "Invalid configuration file!" << std::endl;
      std::exit(EXIT_FAILURE);
   }
   return station;
}

// Referencia a um efeito liberado, mantida apenas para o unref() depois
// que ele expira (gestao de memoria -- releaseOneChaff/Flare devolve um
// ponteiro cuja posse e nossa). Nada aqui alimenta o Tacview: posicao,
// declaracao e remocao no stream vem todas do recorder nativo.
struct ReleasedEffect
{
   mixr::models::Player* player{};
   double releaseSimTime{};
};

} // namespace

int main(int argc, char* argv[])
{
   std::string configFilename = "./poc/09-chaff-flare/configs/scenario.epp";

   for (int i = 1; i < argc; i++) {
      if (std::string(argv[i]) == "-f") configFilename = argv[++i];
   }

   std::signal(SIGINT, onSigint);

   mixr::simulation::Station* station{buildStation(configFilename)};

   station->event(mixr::base::Component::RESET_EVENT);
   station->tcFrame(static_cast<double>(1.0 / static_cast<double>(station->getTimeCriticalRate())));

   const auto hunter = dynamic_cast<mixr::models::Player*>(station->getOwnship());
   if (hunter == nullptr) {
      std::cerr << "No ownship (hunter) found!" << std::endl;
      std::exit(EXIT_FAILURE);
   }

   const auto dynamicsModel = hunter->getDynamicsModel();
   if (dynamicsModel == nullptr) {
      std::cerr << "Hunter has no dynamics model!" << std::endl;
      std::exit(EXIT_FAILURE);
   }
   const int numEngines{dynamicsModel->getNumberOfEngines()};
   const std::vector<double> milPower(static_cast<std::size_t>(numEngines), 1.0);

   const auto storesMgr = hunter->getStoresManagement();
   if (storesMgr == nullptr) {
      std::cerr << "Hunter has no stores manager!" << std::endl;
      std::exit(EXIT_FAILURE);
   }

   std::vector<ReleasedEffect> releasedEffects;

   station->createTimeCriticalProcess();
   mixr::base::msleep(1000);

   const double dt{1.0 / static_cast<double>(bgRate)};
   double simTime{};
   const double startTime{mixr::base::getComputerTime()};
   double nextReleaseTime{5.0}; // primeiro par sai aos 5s (deixa o voo estabilizar)

   std::cout << "=== poc/09-chaff-flare ===" << std::endl;
   std::cout << "(Ctrl+C para encerrar; roda indefinidamente)" << std::endl;

   while (!g_stopRequested) {

      station->updateData(static_cast<double>(dt));

      dynamicsModel->setThrottles(milPower.data(), numEngines);
      {
         // mesmo autonivelamento simples ja usado nas pocs 04/07/08 (F4N
         // comeca destrimado, sem hold-modes nativos do Autopilot).
         const double kP{0.01};
         const double maxInput{0.2};
         const double pitchCorrection{
            std::max(-maxInput, std::min(maxInput, -kP * hunter->getPitchD()))};
         dynamicsModel->setControlStick(0.0, pitchCorrection);
      }

      if (simTime >= nextReleaseTime) {
         nextReleaseTime += releaseIntervalSec;

         mixr::models::Chaff* const chaff{storesMgr->releaseOneChaff()};
         if (chaff != nullptr) {
            releasedEffects.push_back(ReleasedEffect{chaff, simTime});
            std::cout << "[t=" << simTime << "s] CHAFF lancado" << std::endl;
         } else {
            std::cout << "[t=" << simTime << "s] sem chaff disponivel" << std::endl;
         }

         mixr::models::Flare* const flare{storesMgr->releaseOneFlare()};
         if (flare != nullptr) {
            releasedEffects.push_back(ReleasedEffect{flare, simTime});
            std::cout << "[t=" << simTime << "s] FLARE lancado" << std::endl;
         } else {
            std::cout << "[t=" << simTime << "s] sem flare disponivel" << std::endl;
         }
      }

      // Nada de Tacview aqui: hunter, chaff e flare chegam ao stream pela
      // cadeia nativa do 'dataRecorder' (REID_PLAYER_DATA para as posicoes,
      // REID_WEAPON_RELEASED na liberacao, REID_PLAYER_REMOVED quando o
      // Effect expira pelo maxTOF nativo). O ciclo de vida dos efeitos --
      // que este laco antes espelhava a mao, inclusive o "warm-up" de
      // altitude -- agora vem do proprio framework.

      // Libera a nossa referencia depois que o Effect ja expirou pelo
      // maxTOF nativo (nao ha nada a fazer no Tacview aqui).
      for (std::size_t i = 0; i < releasedEffects.size();) {
         if ((simTime - releasedEffects[i].releaseSimTime) >= effectMaxTofSec) {
            releasedEffects[i].player->unref();
            releasedEffects[i] = releasedEffects.back();
            releasedEffects.pop_back();
            continue;
         }
         ++i;
      }

      simTime += dt;
      const double timeNow{mixr::base::getComputerTime()};
      const double elapsedTime{timeNow - startTime};
      const double nextFrameStart{simTime - elapsedTime};
      const int sleepTime{static_cast<int>(nextFrameStart * 1000.0)};
      if (sleepTime > 0) mixr::base::msleep(sleepTime);
   }

   std::cout << "=== fim ===" << std::endl;

   for (auto& fx : releasedEffects) fx.player->unref();

   station->event(mixr::base::Component::SHUTDOWN_EVENT);
   station->unref();
   return 0;
}
