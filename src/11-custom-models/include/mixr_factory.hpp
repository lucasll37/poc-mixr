#pragma once

#include "mixr/base/Object.hpp"

#include <string>

// Factory de objetos MIXR desta PoC: encadeia a factory das classes
// PROPRIAS (mixr::xdrone) antes das do framework, mesmo padrao dos demais
// poc/NN-slug. A primeira que devolver nao-nulo vence.
mixr::base::Object* mixrFactory(const std::string& name);
