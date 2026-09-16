#pragma once

namespace mixr {
namespace base { class String; }
namespace models { class OnboardComputer; }
namespace models {
namespace xC_130 {

//------------------------------------------------------------------------------
// releaseNextStoreOfType() -- acha a proxima estacao livre do StoresMgr do
// player dono de 'mgr' cujo Player::getType() bata com 'storeType', e libera.
// Generico contra mixr::models::AbstractWeapon -- nunca faz dynamic_cast numa
// classe concreta de paraquedista (o mesmo raciocinio documentado em
// ActionParatrooperRelease.hpp).
//
// Extraido de ActionParatrooperRelease::trigger() (unico chamador ate' esta
// extracao) para ser reusado por ActionParatrooperStick, que precisa chamar a
// MESMA busca varias vezes ao longo de process() -- nao so uma vez em
// trigger(). Devolve false (sem lancar, sem abortar) quando nao ha estacao
// livre do tipo pedido, ou quando 'mgr' e' nulo -- o chamador decide o que
// isso significa (ActionParatrooperRelease: falhou o unico disparo;
// ActionParatrooperStick: "acabou a carga", completa a sequencia cedo).
//------------------------------------------------------------------------------
bool releaseNextStoreOfType(models::OnboardComputer* mgr, const base::String& storeType);

} // namespace xC_130
} // namespace models
} // namespace mixr
