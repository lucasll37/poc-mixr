#include "mixr_factory.hpp"

#include "xplugin/PluginRegistry.hpp"
#include "xplugin/factory.hpp"

#include "xtacview/factory.hpp"
#include "xjoystick/factory.hpp"
#include "xmsg/factory.hpp"

#include "mixr/simulation/factory.hpp"
#include "mixr/models/factory.hpp"
#include "mixr/terrain/factory.hpp"
#include "mixr/interop/dis/factory.hpp"
#include "mixr/linkage/factory.hpp"
#include "mixr/recorder/factory.hpp"
#include "mixr/base/factory.hpp"

mixr::base::Object* mixrFactoryBuiltin(const std::string& name)
{
   mixr::base::Object* obj{};

   // 0) classes EDL da carga dinamica: ( PluginLoader ) e ( PluginModule ).
   //    Nao sao as classes VINDAS de plugin -- essas entram por
   //    mixr::xplugin::loadedFactory(), no fim de mixrFactory().
   if (obj == nullptr) obj = mixr::xplugin::factory(name);

   // 1) exportacao para o Tacview (shared/xtacview)
   if (obj == nullptr) obj = mixr::xtacview::factory(name);

   // 2) joystick: JoystickIoHandler (le o UsbJoystick nativo e comanda o
   //    AirVehicle do 'player' configurado)
   if (obj == nullptr) obj = mixr::xjoystick::factory(name);

   // 3) mensagens configuraveis por EDL (shared/xmsg)
   if (obj == nullptr) obj = mixr::xmsg::factory(name);

   // 4) framework -- daqui vem Aircraft, JSBSimModel, Autopilot, DataRecorder...
   if (obj == nullptr) obj = mixr::simulation::factory(name);
   if (obj == nullptr) obj = mixr::models::factory(name);

   // 4) banco de elevacao: SrtmHgtFile. models::factory NAO encadeia esta --
   //    sem a linha abaixo, o 'terrain: ( SrtmHgtFile ... )' do .epp nao
   //    constroi nada e o WorldModel fica sem terreno, em silencio.
   if (obj == nullptr) obj = mixr::terrain::factory(name);

   // 5) DIS nativo -- DisNetIO/DisNtm (namespace real e mixr::dis, apesar
   //    do caminho do header ser mixr/interop/dis/). NAO encadeada por
   //    nenhuma outra factory; sem esta linha o 'networks: ( DisNetIO ... )'
   //    do .epp nao constroi nada, em silencio.
   if (obj == nullptr) obj = mixr::dis::factory(name);

   // 6) UsbJoystick / IoData / AnalogInput -- mixr::linkage nativo, mesma
   //    armadilha do terrain: acima.
   if (obj == nullptr) obj = mixr::linkage::factory(name);

   if (obj == nullptr) obj = mixr::recorder::factory(name);
   if (obj == nullptr) obj = mixr::base::factory(name);

   return obj;
}

//------------------------------------------------------------------------------
// A factory que vai para o edl_parser. NUNCA devolve nullptr.
//
// Ver o cabecalho de mixr_factory.hpp para o porque (resumo: o parser deste
// fork nao reporta nome de fabrica desconhecido -- a mensagem dele e codigo
// morto -- e devolver nullptr termina em SIGSEGV no edl_parser.y:179).
//------------------------------------------------------------------------------
mixr::base::Object* mixrFactory(const std::string& name)
{
   mixr::base::Object* obj{mixrFactoryBuiltin(name)};

   // 8) classes vindas de modelos carregados em tempo de execucao (dlopen).
   //
   //    POR ULTIMO, DE PROPOSITO: assim um plugin so ACRESCENTA nomes, nunca
   //    sombreia em silencio uma classe que ja funciona -- um .so que
   //    acidentalmente chamasse sua classe de "Aircraft" trocaria os avioes
   //    do cenario sem uma linha de aviso.
   //
   //    Mas a ordem NAO e a defesa: colocada por ultimo e so isso, a colisao
   //    deixaria o plugin silenciosamente inerte, que e a patologia de
   //    contexts/BTCPP-CONTEXT.md:2358. A defesa real e a sonda de colisao em
   //    PluginRegistry::loadModule(), que na CARGA pergunta a
   //    mixrFactoryBuiltin() se o nome ja existe e recusa o plugin nomeando os
   //    dois donos. E por isso que mixrFactoryBuiltin() precisa existir
   //    separada.
   if (obj == nullptr) obj = mixr::xplugin::loadedFactory(name);

   if (obj == nullptr) mixr::xplugin::reportUnknownFactoryName(name);  // [[noreturn]]

   return obj;
}
