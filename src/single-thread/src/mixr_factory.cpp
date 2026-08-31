#include "mixr_factory.hpp"

#include "xnative/factory.hpp"
#include "xtacview/factory.hpp"
#include "xclock/factory.hpp"
#include "xjoystick/factory.hpp"
#include "xmsg/factory.hpp"

#include "mixr/simulation/factory.hpp"
#include "mixr/models/factory.hpp"
#include "mixr/terrain/factory.hpp"
#include "mixr/interop/dis/factory.hpp"
#include "mixr/linkage/factory.hpp"
#include "mixr/recorder/factory.hpp"
#include "mixr/base/factory.hpp"

mixr::base::Object* mixrFactory(const std::string& name)
{
   mixr::base::Object* obj{};

   // 1) o pouco que continua sendo nosso (UBF + datalink derivado)
   if (obj == nullptr) obj = mixr::xnative::factory(name);

   // 2) exportacao para o Tacview (shared/xtacview)
   if (obj == nullptr) obj = mixr::xtacview::factory(name);

   // 3) relogio: ClockStation (Station + acelerar/frear/pausar)
   if (obj == nullptr) obj = mixr::xclock::factory(name);

   // 4) joystick: JoystickIoHandler (le o UsbJoystick nativo e comanda o
   //    AirVehicle do 'player' configurado)
   if (obj == nullptr) obj = mixr::xjoystick::factory(name);

   // 5) mensagens configuraveis por EDL: MsgFeed, MsgReport, as condicoes e
   //    os destinos (shared/xmsg)
   if (obj == nullptr) obj = mixr::xmsg::factory(name);

   // 6) framework -- daqui vem Aircraft, JSBSimModel, Autopilot, Gimbal,
   //    Antenna, Tws, AirTrkMgr, SensorMgr, OnboardComputer, SimAgent,
   //    UbfArbiter, SigSphere, DataRecorder...
   if (obj == nullptr) obj = mixr::simulation::factory(name);
   if (obj == nullptr) obj = mixr::models::factory(name);

   // 6) banco de elevacao: SrtmHgtFile / DtedFile / DedFile / QuadMap.
   //    models::factory NAO encadeia esta -- sem a linha abaixo, o
   //    'terrain: ( SrtmHgtFile ... )' do .epp nao constroi nada e o
   //    WorldModel fica sem terreno, em silencio.
   if (obj == nullptr) obj = mixr::terrain::factory(name);

   // 6.5) DIS nativo -- DisNetIO/DisNtm (namespace real e mixr::dis, apesar
   //      do caminho do header ser mixr/interop/dis/). NAO encadeada por
   //      nenhuma outra factory; sem esta linha o 'networks: ( DisNetIO
   //      ... )' do .epp nao constroi nada, em silencio (mesma armadilha
   //      do terrain: acima).
   if (obj == nullptr) obj = mixr::dis::factory(name);

   // 7) UsbJoystick / IoData / AnalogInput -- mixr::linkage nativo. NAO
   //    encadeada por nenhuma das factories acima; sem esta linha o
   //    'devices: { ( UsbJoystick ... ) }' do 'ioHandler:' nao constroi
   //    nada, em silencio (mesma armadilha do terrain: acima).
   if (obj == nullptr) obj = mixr::linkage::factory(name);

   if (obj == nullptr) obj = mixr::recorder::factory(name);
   if (obj == nullptr) obj = mixr::base::factory(name);

   return obj;
}
