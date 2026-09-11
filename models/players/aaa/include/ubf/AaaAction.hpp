#pragma once

#include "mixr/base/ubf/AbstractAction.hpp"

#include <string>

namespace mixr {
namespace base { class Component; }

namespace models {
namespace xaaa {

//------------------------------------------------------------------------------
// Class: AaaAction
//
// Description: A ATUACAO do UBF -- dispara de verdade contra o alvo que a
//              arvore escolheu. Sequencia IDENTICA a' ja usada pelo A-4
//              (ver models/players/A-4/src/ubf/FlightAction.cpp:299-328):
//              e' API generica de mixr::models::Player/StoresMgr/
//              AbstractWeapon, sem nenhum acoplamento ao tipo do lancador
//              -- xmissile::GuidedMissile so' chama getTargetPlayer(),
//              nunca inspeciona quem o lancou. Confirmado nesta mesma
//              sessao de trabalho: a MESMA classe de missil serve tanto ao
//              lancador aereo (A-4) quanto ao terrestre (esta antiaerea),
//              sem nenhuma modificacao.
//
// Factory name: AaaAction
//
// Slots: (nenhum -- rotulo/alvo chegam prontos da arvore, ver
//        ubf::AaaBehavior::genAction())
//
// O que execute() NAO PODE deixar de fazer, em QUALQUER modelo, e' escrever
// no xboard -- ver models/template/docs/CONTRATO.md secao 3. E' a UNICA
// obrigacao que falha em SILENCIO: sem ela, o host sobe, o cenario parseia,
// tudo passa, e a tela de status/o dump '-deterministic' mostram
// 'bt=--'/'dec=0' para sempre, sem erro nenhum em lugar nenhum.
//------------------------------------------------------------------------------
class AaaAction final : public base::ubf::AbstractAction
{
   DECLARE_SUBCLASS(AaaAction, base::ubf::AbstractAction)

public:
   AaaAction();
   AaaAction(const std::string& label, bool fireRequested, const std::string& targetName);

   bool execute(base::Component* actor) override;

private:
   std::string label_{"WATCHING"};
   bool fireRequested_{};
   std::string targetName_;
};

} // namespace xaaa
} // namespace models
} // namespace mixr
