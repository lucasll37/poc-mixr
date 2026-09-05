#pragma once

#include "mixr/base/Component.hpp"

namespace mixr {
namespace xmissile {

//------------------------------------------------------------------------------
// Class: MissileThreadTagProbe
//
// Description: Publica no xboard a thread do pool T/C que esta processando o
//              PLAYER que contem este componente -- sem decidir nada.
//
// Factory name: MissileThreadTagProbe
//
// Slots: nenhum (EMPTY_SLOTTABLE).
//
// Mesma classe/motivo de models/player/flight/xnative/ThreadTagProbe (ver o
// cabecalho de la para a explicacao completa) -- so que aqui dentro do
// plugin 'missile', um projeto Meson INDEPENDENTE (nao linka nem conhece
// models/player/flight). O missil (GuidedMissile) tambem nao tem UBF/agente
// nenhum -- so guiagem propria (domain::Guidance) sobre um dynamicsModel --
// entao sem isto ele mostraria '-' na aba Players do app, exatamente como o
// bandit1 antes desta classe existir.
//
// NOME DIFERENTE DO GEMEO EM flight, DE PROPOSITO -- confirmado quebrando: o
// registro de plugins (shared/xplugin/PluginRegistry.cpp) recusa a carga
// quando DOIS .so's diferentes, abertos no MESMO processo, declaram a MESMA
// classe de factory ("o nome 'ThreadTagProbe' ja foi registrado por
// flight... quem ganharia dependeria da ordem de carga"). Isso acontece
// sempre que um cenario carrega os dois plugins juntos (ex.:
// scenario_intercept_missile.edl.in) -- entao os DOIS nomes tem de ser
// distintos, mesmo a classe sendo, por acaso, identica nos dois lados.
//
// A numeracao em si (o contador que xboard::threadTag() consulta) E'
// compartilhada entre este plugin e o flight, porque mora em shared/xboard
// (uma unica libxboard.so, aberta por dlopen pelos dois) -- e' so o NOME DE
// FABRICA que precisa ser unico por processo, nao a numeracao.
//------------------------------------------------------------------------------
class MissileThreadTagProbe : public base::Component
{
   DECLARE_SUBCLASS(MissileThreadTagProbe, base::Component)

public:
   MissileThreadTagProbe();

protected:
   void updateTC(const double dt = 0.0) override;
};

} // namespace xmissile
} // namespace mixr
