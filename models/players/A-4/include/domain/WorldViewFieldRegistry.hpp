#pragma once

#include "domain/WorldView.hpp"

#include "xrlbridge/FieldRegistry.hpp"

namespace mixr {
namespace models {
namespace xA_4 {
namespace domain {

//------------------------------------------------------------------------------
// worldViewFieldRegistry() -- o catalogo de TODOS os campos numericos/
// booleanos que WorldView expoe (os 38 de XRLBRIDGE_OBSERVATION_FIELDS,
// incluindo RWR/navegacao). Construido uma vez (Meyers singleton, em
// WorldViewFieldRegistry.cpp).
//
// E' o que da aos nos bt_nodes::OnnxPolicyAction/OnnxScoreCondition/
// PyDecideAction a capacidade de resolver um 'schema' (lista nomeada de
// campos, escolhida na porta do XML da arvore) em runtime -- sem abrir mao
// da garantia de compilacao: quem monta este registro expande a MESMA
// XRLBRIDGE_OBSERVATION_FIELDS que libs/xrlbridge/RLBridge.cpp expande
// contra xrlbridge::Observation, entao um nome que divergir entre as duas
// structs continua nao compilando.
//------------------------------------------------------------------------------
const xrlbridge::FieldRegistry<WorldView>& worldViewFieldRegistry();

} // namespace domain
} // namespace xA_4
} // namespace models
} // namespace mixr
