#pragma once

#include "mixr/base/Object.hpp"

#include <string>

// Factory de objetos MIXR desta PoC.
mixr::base::Object* mixrFactory(const std::string& name);
