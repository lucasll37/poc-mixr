// app/src/mixr_factory.cpp e src/node/factory.cpp encadeiam manualmente as mesmas ~10 factories
// nativas na mesma ordem -- decisao deliberada de independencia entre ./app e src/node (ver
// CLAUDE.md, secao "src/node"). Nada garante que as duas cadeias continuem batendo: uma lib nova
// esquecida de um dos dois lados faz um bloco do .edl parar de construir objeto nenhum, em
// silencio, so num dos dois binarios -- a mesma classe de armadilha ja documentada para
// terrain::/dis::/linkage:: nao encadeadas.
//
// Este teste nao levanta Station nenhuma: chama as duas funcoes builtin (a fatia SEM plugin, que
// e onde a divergencia importaria) com um nome sentinela por elo da cadeia e confere que as duas
// produzem o MESMO tipo dinamico -- e que um nome inexistente devolve nulo dos dois lados. As
// duas fontes sao recompiladas aqui, sem lib intermediaria, no mesmo padrao ja usado por
// 'edlcheck' (app/src/meson.build) para reusar a cadeia de producao byte a byte.
#include "mixr_factory.hpp"
#include "factory.hpp"

#include "mixr/base/Object.hpp"

#include <gtest/gtest.h>

#include <string>
#include <typeinfo>
#include <vector>

namespace {

// Um nome por elo da cadeia, na ORDEM em que os dois arquivos os encadeiam (0-8 do comentario de
// mixr_factory.cpp): xplugin, xtacview, xclock, xjoystick, xmsg, simulation, models, terrain,
// dis, linkage, recorder, base. Confirmados lendo o fonte de cada factory.cpp (getFactoryName()
// de cada classe), nao supostos.
const std::vector<std::string> kSentinelas = {
   "PluginLoader",        // libs/xplugin
   "TacviewOutput",       // libs/xtacview
   "ClockStation",        // libs/xclock
   "JoystickIoHandler",   // libs/xjoystick
   "MsgFeed",             // libs/xmsg
   "Station",             // mixr::simulation
   "Aircraft",            // mixr::models
   "QuadMap",             // mixr::terrain
   "DisNetIO",            // mixr::dis (interop/dis)
   "IoData",              // mixr::linkage
   "RecorderFileWriter",  // mixr::recorder (classe FileWriter, factory name != nome de classe)
   "int",                 // mixr::base (classe Integer, factory name != nome de classe)
};

}

TEST(FactoryParity, MesmoNomeProduzMesmoTipoNasDuasCadeias)
{
   for (const auto& nome : kSentinelas) {
      mixr::base::Object* const viaApp{mixrFactoryBuiltin(nome)};
      mixr::base::Object* const viaNode{node::factoryBuiltin(nome)};

      ASSERT_NE(viaApp, nullptr) << "app::mixrFactoryBuiltin nao construiu '" << nome << "'";
      ASSERT_NE(viaNode, nullptr) << "node::factoryBuiltin nao construiu '" << nome << "'";
      EXPECT_EQ(typeid(*viaApp), typeid(*viaNode))
         << "'" << nome << "' produz tipos diferentes em app/ e src/node/ -- "
         << "uma das duas cadeias de factory ficou para tras";

      viaApp->unref();
      viaNode->unref();
   }
}

TEST(FactoryParity, NomeDesconhecidoDevolveNuloNasDuasCadeias)
{
   const std::string nome{"ClasseQueNaoExisteEmFactoryNenhuma123"};
   EXPECT_EQ(mixrFactoryBuiltin(nome), nullptr);
   EXPECT_EQ(node::factoryBuiltin(nome), nullptr);
}
