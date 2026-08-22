#ifndef __xtacview_factory_H__
#define __xtacview_factory_H__

#include <string>

namespace mixr {
namespace base { class Object; }
namespace xtacview {

// Factory da biblioteca xtacview -- encadeie no factory() do main.cpp antes
// das factories do framework (mesmo padrao de shared/x* dos exemplos MIXR).
base::Object* factory(const std::string&);

}
}

#endif
