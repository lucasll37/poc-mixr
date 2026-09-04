//
// shared/xjoystick -- JoystickIoHandler, na camada mais isolada possivel: sem
// Station, sem Simulation, sem AirVehicle, sem dispositivo fisico.
//
// LACUNA DOCUMENTADA, nao resolvida aqui: as armadilhas de verdade desta lib
// (desengate dos hold modes do Autopilot, inversao de sinal do manete,
// numeracao ai:/channel:) so sao exercitadas MANUALMENTE, rodando o binario
// de bandit com hardware de verdade -- ver a secao 'shared/xjoystick' do
// CLAUDE.md raiz. Automatizar isso exigiria um AirVehicle/Autopilot de
// verdade (pesado, e o padrao deste repositorio para esse nivel de teste e a
// suite 'scenario', com Station de verdade) OU fabricar um arquivo de
// dispositivo falso em /dev (invasivo, arriscado em CI e especifico da
// plataforma). Nao promovido a arquitetura nova so para caber um teste --
// documentado aqui como o limite conhecido.
//
// O que ESTE arquivo cobre e o que da para isolar sem nenhuma das duas
// coisas: os setters de slot e o caminho de DEGRADACAO sem hardware -- que
// ate agora nunca teve nenhum teste automatizado (so validacao manual,
// registrada no CLAUDE.md como "testado rodando").
//
#include "xjoystick/JoystickIoHandler.hpp"

#include "mixr/base/String.hpp"
#include "mixr/base/numeric/Integer.hpp"

#include <gtest/gtest.h>

namespace {

using namespace mixr;

TEST(XJoystick, ConstroiEDestroiSemStationSemCrash)
{
   const auto h = new xjoystick::JoystickIoHandler();
   ASSERT_NE(h, nullptr);
   h->unref();
}

// inputDevices()/outputDevices() (a API publica de AbstractIoHandler) tem de
// sobreviver sem 'devices:', sem container e sem hardware -- exatamente o
// estado de um handler recem-construido em C++, antes de qualquer slot do
// EDL ser aplicado. E o mesmo caminho que corre em producao quando NAO ha
// joystick fisico: ver hasRealJoystick() em JoystickIoHandler.cpp.
TEST(XJoystick, DegradaSemHardwareSemContainerSemCrash)
{
   const auto h = new xjoystick::JoystickIoHandler();

   const auto nome = new base::String("bandit1");
   EXPECT_TRUE(h->setSlotByName("player", nome));
   nome->unref();

   const auto indice = new base::Integer(0);
   EXPECT_TRUE(h->setSlotByName("deviceIndex", indice));
   indice->unref();

   // Sem container (nenhum findContainerByType(Station) alcanca nada) e sem
   // /dev/js0 / /dev/input/js0 em CI -- o handler tem de voltar
   // silenciosamente, sem tocar em nenhum AirVehicle (nao ha nenhum).
   EXPECT_NO_FATAL_FAILURE(h->inputDevices(0.02));
   EXPECT_NO_FATAL_FAILURE(h->outputDevices(0.02));

   h->unref();
}

// setSlotByName() ja rejeita ponteiro nulo antes de alcancar o
// setSlotPlayer()/setSlotDeviceIndex() de JoystickIoHandler -- mas o
// contrato exposto (ver o .hpp: 'x == nullptr -> return false') tem de
// continuar valendo se algum dia deixar de ser chamado so pelo parser EDL.
TEST(XJoystick, SetSlotByNameComPonteiroNuloDevolveFalso)
{
   const auto h = new xjoystick::JoystickIoHandler();
   EXPECT_FALSE(h->setSlotByName("player", nullptr));
   EXPECT_FALSE(h->setSlotByName("deviceIndex", nullptr));
   h->unref();
}

TEST(XJoystick, SetSlotByNameComNomeDesconhecidoDevolveFalso)
{
   const auto h = new xjoystick::JoystickIoHandler();
   const auto nome = new base::String("qualquer");
   EXPECT_FALSE(h->setSlotByName("nao-existe", nome));
   nome->unref();
   h->unref();
}

} // namespace
