#include "StationBuilder.hpp"

#include "mixr_factory.hpp"

#include "xplugin/PluginRegistry.hpp"

#include "mixr/simulation/Station.hpp"
#include "mixr/models/WorldModel.hpp"

#include "mixr/base/Component.hpp"
#include "mixr/base/Pair.hpp"
#include "mixr/base/edl_parser.hpp"

#include <stdexcept>

namespace rl {

namespace {

// O registro de plugins do MIXR (libs/xplugin::PluginRegistry) e de escopo
// de PROCESSO INTEIRO -- RTLD_NODELETE, "nunca dlclose" (todo objeto MIXR
// vivo guarda ponteiro pro .data do plugin). Uma vez que este processo
// TENTOU montar uma Station por aqui -- com sucesso OU NAO --, uma segunda
// tentativa nunca e segura: varios caminhos por baixo de edl_parser()/
// xplugin::loadModule() chamam std::exit()/exit() DIRETO (nome de fabrica
// desconhecido, '.so' ausente, o proprio 'loadModule(...) depois do
// parse.' quando xplugin::seal() ja rodou) -- nenhum try/catch deste
// binding alcanca esses caminhos, e eles matam o interprete Python inteiro
// sem excecao nenhuma. CONFIRMADO RODANDO: uma segunda tentativa, mesmo
// depois da PRIMEIRA ter falhado de forma limpa (RuntimeError capturavel,
// ex.: nome de player errado), mata o processo -- um padrao de retry
// perfeitamente razoavel em codigo de treino de RL ("except: corrige e
// tenta de novo") e uma armadilha fatal sem aviso nenhum antes desta
// guarda. Fecha isso NUM SO LUGAR, antes de alcancar edl_parser() uma
// segunda vez -- nao tenta adivinhar quais retentativas seriam "seguras".
bool g_attemptedBuild{false};

} // namespace

mixr::simulation::Station* buildStation(const std::string& filename)
{
   if (g_attemptedBuild) {
      throw std::runtime_error(
         "Este processo ja tentou construir uma Station antes (com sucesso "
         "ou nao) -- o registro de plugins do MIXR e de escopo de processo "
         "inteiro e nao pode ser desfeito/reiniciado. Uma segunda tentativa "
         "aqui mataria o interprete Python sem excecao nenhuma (varios "
         "caminhos de xplugin/edl_parser chamam exit() direto). Use um "
         "processo novo para cada tentativa de montar uma Station.");
   }
   g_attemptedBuild = true;

   // Mesma ordem de app::buildStation(): o registro de plugins precisa
   // conhecer a factory SEM plugin antes do parse, para recusar na carga um
   // plugin cujo nome colida com o framework.
   mixr::xplugin::setBuiltinFactory(mixrFactoryBuiltin);

   int num_errors{};
   mixr::base::Object* obj{mixr::base::edl_parser(filename, mixrFactory, &num_errors)};
   mixr::xplugin::seal();

   if (num_errors > 0) {
      if (obj != nullptr) obj->unref();
      throw std::runtime_error("File: " + filename + ", number of errors: "
                               + std::to_string(num_errors));
   }
   if (obj == nullptr) {
      throw std::runtime_error("Invalid configuration file, no objects defined!");
   }

   const auto pair = dynamic_cast<mixr::base::Pair*>(obj);
   if (pair != nullptr) {
      obj = pair->object();
      obj->ref();
      pair->unref();
   }

   const auto station = dynamic_cast<mixr::simulation::Station*>(obj);
   if (station == nullptr) {
      obj->unref();
      throw std::runtime_error("Invalid configuration file!");
   }
   return station;
}

void primeStation(mixr::simulation::Station* const station)
{
   station->event(mixr::base::Component::RESET_EVENT);
   station->tcFrame(1.0 / static_cast<double>(station->getTimeCriticalRate()));
}

mixr::models::WorldModel* worldModelOf(mixr::simulation::Station* const station)
{
   const auto worldModel = dynamic_cast<mixr::models::WorldModel*>(station->getSimulation());
   if (worldModel == nullptr) {
      throw std::runtime_error("No WorldModel found!");
   }
   return worldModel;
}

} // namespace rl
