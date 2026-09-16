#ifndef __xterrain_factory_H__
#define __xterrain_factory_H__

#include <string>

namespace mixr {
namespace base { class Object; }
namespace xterrain {

// Factory da biblioteca xterrain -- encadeie no factory() do main.cpp junto
// de mixr::terrain::factory() (mesmo padrao de libs/x* dos exemplos MIXR):
// a primeira que retorna nao-nulo vence, e "MultiTileTerrain" e' um nome
// nosso, nao do framework.
base::Object* factory(const std::string&);

}
}

#endif
