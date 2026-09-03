#include "app/StationBuilder.hpp"

#include "mixr_factory.hpp"

#include "xplugin/PluginRegistry.hpp"

#include "xclock/ClockStation.hpp"
#include "xtacview/ExposedDataRecorder.hpp"
#include "xtacview/TacviewOutput.hpp"

#include "mixr/linkage/IoHandler.hpp"

#include "mixr/recorder/OutputHandler.hpp"

#include "mixr/simulation/Station.hpp"
#include "mixr/models/WorldModel.hpp"

#include "mixr/base/Component.hpp"
#include "mixr/base/Pair.hpp"
#include "mixr/base/edl_parser.hpp"

#include <cstdlib>
#include <iostream>

namespace app {

mixr::simulation::Station* buildStation(const std::string& filename)
{
   // O registro de plugins precisa saber o que a aplicacao JA constroi sem
   // plugin nenhum: e assim que ele recusa, na CARGA, um plugin cujo nome de
   // fabrica colida com o framework -- em vez de deixa-lo silenciosamente
   // inerte. Este e o unico arquivo que enxerga as duas pontas (a factory da
   // poc e o registro).
   mixr::xplugin::setBuiltinFactory(mixrFactoryBuiltin);

   int num_errors{};

   // A carga dos plugins acontece AQUI DENTRO, durante o parse: o
   // ( PluginLoader ) declarado no .epp faz o dlopen no proprio isValid(),
   // que o parser chama no fecha-parenteses do bloco -- antes, portanto, de
   // qualquer forma escrita depois dele no arquivo. Ver o cabecalho de
   // shared/xplugin/PluginLoader.hpp para a prova de ordem.
   mixr::base::Object* obj{mixr::base::edl_parser(filename, mixrFactory, &num_errors)};

   // Fecha a janela de escrita do registro: dai em diante ele e so leitura,
   // e uma carga tardia (ja com as threads de tempo critico no ar) vira erro
   // em vez de corrida.
   mixr::xplugin::seal();

   if (num_errors > 0) {
      std::cerr << "File: " << filename << ", number of errors: " << num_errors << std::endl;
      std::exit(EXIT_FAILURE);
   }
   if (obj == nullptr) {
      std::cerr << "Invalid configuration file, no objects defined!" << std::endl;
      std::exit(EXIT_FAILURE);
   }

   // O topo do arquivo e um Pair (nome: objeto); quem interessa e o objeto.
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

void primeStation(mixr::simulation::Station* const station)
{
   station->event(mixr::base::Component::RESET_EVENT);
   station->tcFrame(1.0 / static_cast<double>(station->getTimeCriticalRate()));
}

mixr::models::WorldModel* worldModelOf(mixr::simulation::Station* const station)
{
   const auto worldModel = dynamic_cast<mixr::models::WorldModel*>(station->getSimulation());
   if (worldModel == nullptr) {
      std::cerr << "No WorldModel found!" << std::endl;
      std::exit(EXIT_FAILURE);
   }
   return worldModel;
}

mixr::xclock::ClockStation* clockStationOf(mixr::simulation::Station* const station)
{
   const auto clockStation = dynamic_cast<mixr::xclock::ClockStation*>(station);
   if (clockStation == nullptr) {
      std::cerr << "[main] aviso: a Station do cenario nao e uma ( ClockStation );"
                << " controle de tempo desligado" << std::endl;
   }
   return clockStation;
}

mixr::linkage::IoHandler* ioHandlerOf(mixr::simulation::Station* const station)
{
   const auto ioHandler = dynamic_cast<mixr::linkage::IoHandler*>(station->getIoHandler());
   if (ioHandler == nullptr) {
      std::cerr << "[main] aviso: cenario sem ( JoystickIoHandler ) no slot 'ioHandler:';"
                << " controle por joystick desligado" << std::endl;
   }
   return ioHandler;
}

mixr::xtacview::TacviewOutput* tacviewOutputOf(mixr::simulation::Station* const station)
{
   // Navegacao PARA BAIXO na arvore (slot -> filho) -- funciona; e o sentido
   // contrario (filho -> pai, container()) que esta quebrado, documentado em
   // TacviewOutput::resolveInfo(). getOutputHandler() so e alcancavel de
   // fora porque o .epp declara ( ExposedDataRecorder ) em vez de
   // ( DataRecorder ) -- ver o cabecalho de ExposedDataRecorder.hpp.
   const auto dataRecorder = dynamic_cast<mixr::xtacview::ExposedDataRecorder*>(station->getDataRecorder());
   mixr::recorder::OutputHandler* const outputHandler{
      dataRecorder != nullptr ? dataRecorder->getOutputHandler() : nullptr};
   mixr::base::Pair* const pair{
      outputHandler != nullptr ? outputHandler->findByType(typeid(mixr::xtacview::TacviewOutput)) : nullptr};
   const auto tacviewOutput = pair != nullptr
      ? dynamic_cast<mixr::xtacview::TacviewOutput*>(pair->object()) : nullptr;

   if (tacviewOutput == nullptr) {
      std::cerr << "[main] aviso: cenario sem ( TacviewOutput ) na cadeia do dataRecorder;"
                << " varredura do radar nao sera exportada para o Tacview" << std::endl;
   }
   return tacviewOutput;
}

} // namespace app
