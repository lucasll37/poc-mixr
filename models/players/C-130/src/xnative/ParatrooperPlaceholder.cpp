#include "xnative/ParatrooperPlaceholder.hpp"

#include "mixr/base/String.hpp"

namespace mixr {
namespace models {
namespace xC_130 {

IMPLEMENT_SUBCLASS(ParatrooperPlaceholder, "C130ParatrooperPlaceholder")
EMPTY_SLOTTABLE(ParatrooperPlaceholder)
EMPTY_COPYDATA(ParatrooperPlaceholder)
EMPTY_DELETEDATA(ParatrooperPlaceholder)

const char* ParatrooperPlaceholder::getDescription() const   { return "Paratrooper (placeholder)"; }
const char* ParatrooperPlaceholder::getNickname() const      { return "Paratrooper"; }

// Tipo default "PARATROOPER" -- bate com o default de
// ActionParatrooperRelease::storeType_ (xnative/ActionParatrooperRelease.hpp)
// mesmo se o EDL nao declarar 'type:' na estacao.
ParatrooperPlaceholder::ParatrooperPlaceholder()
{
   STANDARD_CONSTRUCTOR()

   static base::String generic("PARATROOPER");
   setType(&generic);
}

//------------------------------------------------------------------------------
// dynamics() -- o PONTO DE SAIDA (ver o comentario do cabecalho para o
// "porque"). Enquanto o placeholder esta em PRE_RELEASE (o estado em que
// 'AbstractWeapon::release()' poe o clone recem criado, entre a chamada de
// liberacao e o primeiro frame como ACTIVE), 'AbstractWeapon::dynamics()' le
// 'initXPos'/'initYPos'/'initAlt' nao como posicao no terreno de jogo, e sim
// como deslocamento em eixos do CORPO da aeronave lancadora (x=nariz,
// y=asa direita, altitude +para cima). Sem fixar os dois aqui, os tres ficam
// em zero (o default de um player que nunca os declarou) e o placeholder
// nasce EXATAMENTE na posicao do C-130 -- e' o que fazia os paraquedistas
// liberados por src/poc/c130-airdrop parecerem colidir com a aeronave no
// Tacview no instante da largada.
//------------------------------------------------------------------------------
void ParatrooperPlaceholder::dynamics(const double dt)
{
   if (isMode(PRE_RELEASE) && getLaunchVehicle() != nullptr) {
      setInitPosition(-15.0, 0.0);   // x=+nariz  =>  atras e' negativo
      setInitAltitude(-10.0);        // +para cima  =>  abaixo e' negativo
   }

   BaseClass::dynamics(dt);
}

} // namespace xC_130
} // namespace models
} // namespace mixr
