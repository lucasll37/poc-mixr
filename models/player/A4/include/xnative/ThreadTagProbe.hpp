#ifndef __xnative_ThreadTagProbe_H__
#define __xnative_ThreadTagProbe_H__

#include "mixr/base/Component.hpp"

namespace mixr {
namespace xnative {

//------------------------------------------------------------------------------
// Class: ThreadTagProbe
//
// Description: Publica no xboard a thread do pool T/C que esta processando o
//              PLAYER que contem este componente -- sem decidir nada.
//
// Factory name: ThreadTagProbe
//
// Slots: nenhum (EMPTY_SLOTTABLE) -- e' um componente generico, sem parametro.
//
// PARA QUE SERVE. FlightAgentTC ja publica o thread tag, mas so como efeito
// colateral de DECIDIR (controller(), fase 3). Um player sem agente -- ex.:
// bandit1 no app/, um intruso so pilotado por ( Autopilot ) -- nunca chama
// FlightAgentTC nem FlightAction::execute(), entao nunca escreve no xboard, e
// a coluna 'thread' da aba Players sai '-' para sempre. Isso e' honesto quanto
// a "nenhuma decisao acontece", mas o player AINDA E processado pelo pool
// T/C: suas 4 fases (dinamica/transmissao/recepcao/processo) rodam, a cada
// frame, na MESMA thread que a Simulation atribuiu a ele (round-robin por
// posicao na lista de players -- ver Simulation::updateTcPlayerList()). Este
// componente so torna essa thread OBSERVAVEL, sem fingir nenhuma decisao.
//
// Mesmo mecanismo generico de FlightAgentTC (ver o cabecalho dele): o player
// e' percorrido 4x por frame (Player::updateTC() chama BaseClass::updateTC()
// -- Component::updateTC() -- em toda fase, e esta recursao generica e' quem
// invoca tcFrame() de QUALQUER componente da lista, tenha ele tipo especial ou
// nao). Sem filtro de fase o mesmo valor seria escrito 4x por frame a toa; por
// isso, no mesmo espirito de FlightAgentTC::controller(), so na fase 3.
//------------------------------------------------------------------------------
class ThreadTagProbe : public base::Component
{
   DECLARE_SUBCLASS(ThreadTagProbe, base::Component)

public:
   ThreadTagProbe();

protected:
   void updateTC(const double dt = 0.0) override;
};

} // namespace xnative
} // namespace mixr

#endif
