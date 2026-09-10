#pragma once

#include "mixr/models/player/effect/Effect.hpp"

namespace mixr {
namespace models {
namespace xC_130 {

//------------------------------------------------------------------------------
// Class: ParatrooperPlaceholder
//
// Description: Entidade nativa PROVISORIA para o slot 'stores:' do C-130,
//              ate o modelo de producao models/players/paratrooper existir de
//              verdade. Nenhum comportamento proprio -- e um 'Effect' liso
//              (mesma familia nativa de Chaff/Decoy/Flare), que existe so
//              para o EDL ter algo construivel do tipo "PARATROOPER" que
//              ActionParatrooperRelease possa liberar.
//
//              Nome de fabrica PROPOSITALMENTE diferente de "Paratrooper"
//              (ver docs/ARCHITECTURE.md): registrar o mesmo nome que o
//              futuro modelo de producao vai usar colidiria com
//              tests/guard/check_colisao_fabrica.py assim que aquele modelo
//              nascer -- a troca futura e so EDL (a estacao + 'provides:'),
//              nunca precisa desta classe deixar de existir com este nome.
//
// Factory name: C130ParatrooperPlaceholder
//
// Slots: (nenhum -- herda 'dragIndex' de Effect e id/side/type/dataLogTime/
//        maxTOF de AbstractWeapon/Player)
//------------------------------------------------------------------------------
class ParatrooperPlaceholder final : public mixr::models::Effect
{
   DECLARE_SUBCLASS(ParatrooperPlaceholder, mixr::models::Effect)

public:
   ParatrooperPlaceholder();

   const char* getDescription() const override;
   const char* getNickname() const override;
};

} // namespace xC_130
} // namespace models
} // namespace mixr
