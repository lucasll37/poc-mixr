#pragma once

#include "xmsg/Snapshot.hpp"

#include <string>

namespace mixr {
namespace models { class Player; }

namespace xmsg {

//------------------------------------------------------------------------------
// SnapshotSource -- o UNICO lugar que le o Player.
//
// Toda a fronteira com o MIXR do lado da amostragem esta aqui. O resto do
// subsistema (condicoes, mensagens, sinks) trabalha sobre o Snapshot, que e
// numeros crus -- e por isso a logica de deteccao pode ser testada sem
// framework nenhum (ver rules/ e tests/domain/test_xmsg_rules.cpp).
//
// A MASCARA DE GRUPOS NAO E COSMETICA. Os acessores cinematicos do Player sao
// inline; os de motor descem no FGPropulsion do JSBSim e iteram os motores um
// a um. Quem nao pede motor nao paga motor -- e num cenario com dezenas de
// players isso e a diferenca entre custo desprezivel e custo visivel no laco.
//------------------------------------------------------------------------------

constexpr unsigned groupBit(const Group g)  { return 1u << static_cast<int>(g); }

// Preenche 'snap' a partir de 'p', so nos grupos pedidos em 'groupMask'.
// Grupo nao pedido, ou indisponivel neste player, fica com validade false --
// e a diferenca entre "o valor e zero" e "a grandeza nao existe aqui".
//
// 'p' e nao-const porque getTrackManagerByName() nao e const no framework
// (mesmo const_cast confinado de xnative/TrackQuery.cpp).
void fillSnapshot(Snapshot& snap, models::Player* p, unsigned groupMask,
                  const std::string& trackManagerName);

} // namespace xmsg
} // namespace mixr
