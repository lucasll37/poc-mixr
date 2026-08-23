#ifndef __xair_ubf_FlightAgent_H__
#define __xair_ubf_FlightAgent_H__

#include "mixr/base/ubf/Agent.hpp"

#include <atomic>

namespace mixr {
namespace xair {

//------------------------------------------------------------------------------
// Class: FlightAgent
//
// Description: O agente do UBF desta poc -- roda o ciclo
//              percepcao/decisao/atuacao na FASE 3 do frame de tempo
//              critico, para a aeronave que o contem.
//
// Factory name: FlightAgent
//
// Slots: state <AbstractState>, behavior <AbstractBehavior>  (herdados de Agent)
//
// DUAS ARMADILHAS DO FRAMEWORK QUE ESTA CLASSE RESOLVE:
//
//  1) 'UbfAgentTC' NAO E REGISTRADO por nenhuma factory do MIXR
//     (base/factory.cpp registra apenas UbfAgent e UbfArbiter). Escrever
//     ( UbfAgentTC ... ) no EDL falha silenciosamente -- a menos que a
//     aplicacao registre a classe na sua propria factory, que e o que
//     xair::factory() faz com esta subclasse.
//
//  2) AgentTC::updateTC() chama controller(dt) em TODA fase do frame (o
//     Component e atualizado 4x por frame, uma por fase, com dt/4). Rodar
//     a decisao 4 vezes -- e nas fases erradas -- nao e o que se quer.
//     Aqui controller() e filtrado: so roda na fase 3 ("logica e
//     controle"), e com o dt do frame INTEIRO (dt*4), exatamente como
//     models::System faz com as suas quatro fases.
//
// O ator e o player que contem o agente (initActor()), entao o mesmo bloco
// EDL serve para qualquer aeronave -- diferente do SimAgent nativo, que
// mora na Station e amarra o ator por NOME.
//------------------------------------------------------------------------------
class FlightAgent : public base::ubf::AgentTC
{
   DECLARE_SUBCLASS(FlightAgent, base::ubf::AgentTC)

public:
   FlightAgent();

   long getDecisionCount() const   { return decisions.load(std::memory_order_relaxed); }
   int getLastThreadTag() const    { return lastThreadTag.load(std::memory_order_relaxed); }

protected:
   void controller(const double dt) override;
   void initActor() override;

private:
   std::atomic<long> decisions{};
   std::atomic<int> lastThreadTag{-1};
};

} // namespace xair
} // namespace mixr

#endif
