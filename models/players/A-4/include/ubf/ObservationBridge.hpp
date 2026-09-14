#pragma once

#include "domain/WorldView.hpp"

#include "xrlbridge/RLBridge.hpp"

namespace mixr {
namespace models {
namespace xA_4 {

//------------------------------------------------------------------------------
// toObservation() -- domain::WorldView -> xrlbridge::Observation, expandindo
// a MESMA macro que libs/xrlbridge/RLBridge.cpp expande contra
// xrlbridge::Observation (ver xrlbridge/ObservationFields.hpp): um nome que
// divergir entre as duas structs nao compila.
//
// Extraida do anonimo de RLBridgeBehavior.cpp (onde morava antes, como uma
// copia campo a campo A MAO -- o elo mais fraco do contrato, sem essa
// garantia de compilacao) para um arquivo proprio, testavel isoladamente:
// FlightState::snap e' privado, sem setter, entao nao ha outro seam de
// injecao nesta camada sem um AirVehicle de verdade (ver
// tests/native/test_rl_bridge_behavior.cpp).
//------------------------------------------------------------------------------
xrlbridge::Observation toObservation(const domain::WorldView& snap);

} // namespace xA_4
} // namespace models
} // namespace mixr
