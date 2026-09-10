#include "xnative/factory.hpp"

#include "ubf/Navstar3Action.hpp"
#include "ubf/Navstar3BtBehavior.hpp"
#include "ubf/Navstar3State.hpp"
#include "xnative/Navstar3AgentTC.hpp"

#include "mixr/base/MetaObject.hpp"
#include "mixr/base/Object.hpp"

namespace mixr {
namespace models {
namespace xNavstar_3 {

//------------------------------------------------------------------------------
// Registra as quatro classes deste modelo. Ao acrescentar uma classe nova,
// ela entra em TRES lugares, sempre juntos: aqui (o if/else), em NOMES[] e em
// METAS[] logo abaixo -- o registro de plugin confere as tres listas na
// carga e recusa se ficarem fora de sincronia.
//
// Nao ha classe de "corpo fisico" propria aqui (ao contrario de
// models/players/paratrooper, que precisou de xnative::Paratrooper) --
// ( SpaceVehicle ) e' construivel DIRETO pela factory nativa mixr::models
// (mixr::models::factory.cpp despacha o nome "SpaceVehicle" nativamente),
// entao este modelo so precisa da percepcao/decisao/atuacao/agente.
//
// Prefixo 'Navstar3' em TODOS os nomes de fabrica, de proposito -- nomes de
// fabrica sao GLOBAIS ao processo (nao por-plugin): rodar
// python3 tests/guard/check_colisao_fabrica.py apos qualquer mudanca aqui.
//------------------------------------------------------------------------------
base::Object* factory(const std::string& name)
{
   base::Object* obj{};

   if (name == Navstar3State::getFactoryName())         obj = new Navstar3State();
   else if (name == Navstar3BtBehavior::getFactoryName()) obj = new Navstar3BtBehavior();
   else if (name == Navstar3Action::getFactoryName())    obj = new Navstar3Action();
   else if (name == Navstar3AgentTC::getFactoryName())   obj = new Navstar3AgentTC();

   return obj;
}

namespace {

const char* const NOMES[] = {
   "Navstar3State",
   "Navstar3BtBehavior",
   "Navstar3Action",
   "Navstar3AgentTC",
   nullptr
};

const base::MetaObject* const METAS[] = {
   Navstar3State::getMetaObject(),
   Navstar3BtBehavior::getMetaObject(),
   Navstar3Action::getMetaObject(),
   Navstar3AgentTC::getMetaObject(),
   nullptr
};

} // namespace

const char* const* factoryNames()             { return NOMES; }
const base::MetaObject* const* metaObjects()  { return METAS; }

} // namespace xNavstar_3
} // namespace models
} // namespace mixr
