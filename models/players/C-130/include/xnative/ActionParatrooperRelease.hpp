#pragma once

#include "mixr/models/Actions.hpp"

#include "mixr/base/String.hpp"

namespace mixr {
namespace models {
namespace xC_130 {

//------------------------------------------------------------------------------
// Class: ActionParatrooperRelease
//
// Description: Libera a proxima estacao de STORES cujo tipo (slot 'type:',
//              Player::getType()) bata com 'storeType' -- generico contra
//              mixr::models::AbstractWeapon, nunca contra uma classe
//              concreta. E o mecanismo de "lancamento de paraquedista": o
//              paraquedista e tratado como uma ARMA liberavel (mesma familia
//              de Bomb/Decoy/Chaff), nao como um Player autonomo com UBF
//              proprio -- ele nao decide nada, so cai.
//
// Factory name: C130ActionParatrooperRelease
//
// Slots:
//    storeType <String> ! O valor do slot 'type:' que identifica uma estacao
//                       ! como paraquedista (default: "PARATROOPER")
//
// Mesmo caminho nativo de trigger() que ActionWeaponRelease/
// ActionDecoyRelease ja usam (ver mixr::models::Route::triggerAction(),
// chamado quando a rota passa pelo steerpoint): mgr->findContainerByType
// acha o Player dono, Player::getStoresManagement() acha o StoresMgr, e
// StoresMgr::getWeapons() devolve TODAS as estacoes que sao AbstractWeapon
// (bombas, decoys, o placeholder do paraquedista, misturados). Filtrar por
// getType() (a string do slot 'type:' do EDL) -- em vez de dynamic_cast numa
// classe concreta -- e o que deixa a troca futura pelo paratrooper de
// verdade (models/players/paratrooper, tambem AbstractWeapon/Effect) livre
// de mudanca de C++ aqui: so o EDL muda (a classe declarada na estacao +
// 'provides:').
//
// Nunca lanca, nunca aborta: sem estacao livre do tipo pedido, trigger()
// devolve false (laps extras sobre o mesmo steerpoint, com wrap:true, viram
// no-op inofensivo -- "acabou a carga").
//------------------------------------------------------------------------------
class ActionParatrooperRelease final : public mixr::models::Action
{
   DECLARE_SUBCLASS(ActionParatrooperRelease, mixr::models::Action)

public:
   ActionParatrooperRelease();

   bool trigger(OnboardComputer* const mgr) override;

private:
   base::String storeType_{"PARATROOPER"};

   bool setSlotStoreType(const base::String* const);
};

} // namespace xC_130
} // namespace models
} // namespace mixr
