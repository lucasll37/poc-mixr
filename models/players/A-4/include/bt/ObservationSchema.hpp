#pragma once

#include "xrlbridge/Schema.hpp"

#include <string>

namespace mixr {
namespace models {
namespace xA_4 {
namespace bt_nodes {

//------------------------------------------------------------------------------
// resolveObservationSchema() -- traduz o valor de uma porta 'schema' (dos
// nos OnnxPolicyAction/OnnxScoreCondition/PyDecideAction) num
// xrlbridge::Schema:
//
//   ""/"classic28"   -> xrlbridge::classicSchema28() (os 28 nomes historicos,
//                       na ordem historica -- o default de todo no)
//   "all"            -> todo campo de domain::worldViewFieldRegistry(), na
//                       ordem em que foi registrado (os 38 completos)
//   qualquer outro   -> lista ad-hoc, nomes separados por espaco
//                       (ex.: "northM eastM hasContact")
//
// NAO valida os nomes contra o registro -- isso e' papel de
// xrlbridge::bind(), chamado por quem consome o Schema devolvido aqui (o
// erro de nome desconhecido precisa saber CONTRA QUAL registro resolver,
// que este arquivo nao sabe).
//
// Fica FORA de domain/ (que nao pode chamar xrlbridge::classicSchema28() --
// um simbolo de libxrlbridge.so, nao header-only -- sem arrastar sdk_dep
// para dentro de test_domain/test_tree, que hoje so' usam os HEADERS do SDK.
// Ver o comentario de bt_sdk_sources em meson.build).
//------------------------------------------------------------------------------
xrlbridge::Schema resolveObservationSchema(const std::string& value);

} // namespace bt_nodes
} // namespace xA_4
} // namespace models
} // namespace mixr
