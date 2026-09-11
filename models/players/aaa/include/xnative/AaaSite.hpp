#pragma once

#include "mixr/models/player/ground/SamVehicle.hpp"

namespace mixr {
namespace models {
namespace xaaa {

//------------------------------------------------------------------------------
// Class: AaaSite
//
// Description: Uma antiaerea (bateria de superficie-ar) estacionaria.
//              Subclasse de mixr::models::SamVehicle SO' para herdar os
//              slots nativos minLaunchRange/maxLaunchRange (o "domo" de
//              alcance de engajamento, ja' nativo do MIXR) -- nenhum
//              comportamento novo aqui. A decisao inteira (percepcao ->
//              arvore -> disparo) mora em ubf::AaaState/AaaBehavior/
//              AaaAction, hospedados por um ( UbfAgent ) NATIVO (nao uma
//              subclasse propria) declarado dentro dos components: desta
//              classe no .edl do cenario.
//
// Factory name: AaaSite
//
// Slots: (nenhum proprio -- minLaunchRange/maxLaunchRange/etc ja' vem de
//        SamVehicle/GroundVehicle)
//
// POR QUE NAO HA UMA SUBCLASSE DE AgentTC AQUI (ao contrario de
// models/players/A-4/include/xnative/FlightAgentTC.hpp): a variante de
// TEMPO CRITICO (mixr::base::ubf::AgentTC, fase 3 do frame, factory name
// "UbfAgentTC") NAO e' registrada por nenhuma factory nativa do MIXR --
// so' a variante de FUNDO (mixr::base::ubf::Agent, updateData() a' taxa de
// fundo, factory name "UbfAgent") e' (confirmado em
// contexts/src/mixr/src/base/ubf/Agent.cpp: IMPLEMENT_SUBCLASS(Agent,
// "UbfAgent") / IMPLEMENT_SUBCLASS(AgentTC, "UbfAgentTC"), e so' a primeira
// aparece encadeada em base/factory.cpp). Uma antiaerea estacionaria nao
// precisa decidir a' taxa de tempo critico (50 Hz) -- um alvo a
// ~100 m/s entra no domo e ha' de sobra a taxa de fundo (10 Hz, ~100 ms de
// latencia) para "alcance dentro do domo -> dispara". Por isso o .edl deste
// cenario declara ( UbfAgent state: (AaaState) behavior: (AaaBehavior) )
// DIRETO, nativo, sem nenhum C++ novo -- e' o que
// Agent::initActor()/controller() ja fazem sozinhos
// (contexts/src/mixr/src/base/ubf/Agent.cpp): o ator vira' o proprio
// container() do Agent, que e' esta classe quando 'agent:' e' declarado
// dentro de components: dela.
//
// ARMADILHA CONFIRMADA, NAO REDESCOBRIR (ver ubf/AaaState.hpp/.cpp):
// SamVehicle::updateData() conta municao via dynamic_cast<const Sam*> --
// xmissile::GuidedMissile (models/players/missile) e' IRMA de
// mixr::models::Sam (as duas derivam de Missile diretamente, nao uma da
// outra), entao SamVehicle::getNumberOfMissiles()/isLauncherReady() ficam
// SEMPRE falso/zero com um ( GuidedMissile ) no stores:, em silencio.
// ubf::AaaState usa StoresMgr::available() > 0 diretamente, nunca
// isLauncherReady().
//
// ESTACIONARIA POR CONFIGURACAO DE .edl, nao por codigo: sem
// dynamicsModel: e com initVelocity:0.0 a aeronave/veiculo fica parado --
// Player::dynamics() so' invoca um modelo dinamico se
// getDynamicsModel() != nullptr (Player.cpp), e a integracao de posicao
// roda de qualquer forma mas com velocidade zero nada se move. Mesmo idioma
// ja' usado por src/poc/paratrooper-drop (o Paratrooper parado).
//------------------------------------------------------------------------------
class AaaSite final : public SamVehicle
{
   DECLARE_SUBCLASS(AaaSite, SamVehicle)

public:
   AaaSite();
};

} // namespace xaaa
} // namespace models
} // namespace mixr
