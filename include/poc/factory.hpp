#pragma once
#include <string>
namespace mixr { namespace base { class Object; } }
namespace poc {
    mixr::base::Object* factory(const std::string& name);
}
