#pragma once

#include "mixr/base/Object.hpp"

#include <string>

// Factory de objetos MIXR desta PoC: encadeia a factory das classes
// PROPRIAS (mixr::xair) antes das do framework.
mixr::base::Object* mixrFactory(const std::string& name);
