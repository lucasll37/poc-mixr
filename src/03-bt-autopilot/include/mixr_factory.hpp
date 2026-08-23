#pragma once

#include "mixr/base/Object.hpp"

#include <string>

// Factory de objetos MIXR desta PoC: encadeia as factories das bibliotecas
// do framework que usamos (mesmo padrao dos demais poc/NN-slug).
mixr::base::Object* mixrFactory(const std::string& name);
