#pragma once

#include "mixr/base/Object.hpp"

#include <string>

// Factory de objetos MIXR desta PoC: encadeia as factories das bibliotecas
// do framework que usamos. Novidade em relacao as pocs anteriores:
// mixr::recorder (DataRecorder/NetOutput/TabPrinter) e mixr::linkage
// (KeyboardDevice/IoHandler), nenhuma das duas usada ate agora no projeto.
mixr::base::Object* mixrFactory(const std::string& name);
