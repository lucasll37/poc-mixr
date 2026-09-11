#pragma once

#include "mixr/models/player/effect/Effect.hpp"

namespace mixr {
namespace models {
namespace xC_130 {

//------------------------------------------------------------------------------
// Class: ParatrooperPlaceholder
//
// Description: Fixture de bancada para os testes nativos deste modelo
//              (tests/native/test_paratrooper_release.cpp e
//              test_paratrooper_stick.cpp, que constroem esta classe direto
//              para exercitar ActionParatrooperRelease/ActionParatrooperStick
//              sem depender de outro plugin). NAO e mais usada por nenhum
//              cenario de producao -- src/poc/c130-airdrop passou a liberar
//              o 'Paratrooper' real (models/players/paratrooper, FSM
//              completa: queda livre, paraquedas, pouso), troca que foi so'
//              EDL (a classe da estacao + 'provides:' de libparatrooper.so),
//              exatamente como esta classe ja previa desde que foi escrita.
//              E um 'Effect' liso (mesma familia nativa de Chaff/Decoy/Flare)
//              -- existe so para o EDL/os testes terem algo construivel do
//              tipo "PARATROOPER" que ActionParatrooperRelease possa liberar.
//
//              Mantida (nome de fabrica incluso, nao removida) para os testes
//              nativos deste modelo nao precisarem linkar libparatrooper.so
//              -- cada modelo sob models/ e um projeto Meson autocontido (ver
//              CLAUDE.md raiz, "O MODELO e um plugin"). Nome de fabrica
//              PROPOSITALMENTE diferente de "Paratrooper" (ver
//              docs/ARCHITECTURE.md): registrar o mesmo nome que o modelo de
//              producao usa colidiria com tests/guard/check_colisao_fabrica.py
//              assim que os dois `.so` carregassem juntos no mesmo processo
//              (como ja acontece em src/poc/c130-airdrop hoje).
//
//              UMA sobrescrita, dynamics() -- o MESMO ponto de saida que
//              models/players/paratrooper/src/xnative/Paratrooper.cpp usa
//              (mesmo comentario la para o "porque"): sem isto, o mecanismo
//              nativo de AbstractWeapon::dynamics() em PRE_RELEASE aplica
//              offset ZERO, e o placeholder nasce colado na posicao exata do
//              C-130 -- no Tacview, o instante da liberacao parece uma
//              colisao com a aeronave (achado rodando src/poc/c130-airdrop).
//              15 m atras / 10 m abaixo, fixos aqui sem slot (esta classe e
//              so um placeholder provisorio) -- os mesmos defaults do modelo
//              real.
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

protected:
   void dynamics(const double dt) override;
};

} // namespace xC_130
} // namespace models
} // namespace mixr
