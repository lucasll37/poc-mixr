#pragma once

#include "mixr/base/Object.hpp"

#include <string>

// Factory de objetos MIXR desta PoC: tenta primeiro as classes proprias
// (RadarContactRelay, AlertReceiver), cai pras factories do framework.
mixr::base::Object* mixrFactory(const std::string& name);
