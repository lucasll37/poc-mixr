#pragma once

#include "mixr/base/ubf/Agent.hpp"

#include <atomic>

namespace mixr {
namespace models {
namespace xparatrooper {

//------------------------------------------------------------------------------
// Class: ParatrooperAgentTC
//
// Description: O UNICO agente deste modelo. Roda o ciclo do UBF (percepcao/
//              decisao/atuacao) na FASE 3 do frame de TEMPO CRITICO, para o
//              player que o contem. Adaptado (nao linkado) de
//              models/players/air/A-4/include/xnative/FlightAgentTC.hpp e
//              models/players/air/C-130/include/xnative/FlightAgentTC.hpp.
//
// Factory name: ParatrooperAgentTC
//
// Slots: nenhum proprio. 'state'/'behavior' sao herdados de ubf::Agent.
//
// QUATRO ARMADILHAS DO FRAMEWORK QUE ESTA CLASSE RESOLVE
//
//  1) 'AgentTC' NAO E CONSTRUIDO POR NENHUMA FACTORY DO MIXR -- registro
//     manual em xnative::factory().
//  2) AgentTC::updateTC() chama controller() em TODA fase -- aqui so roda
//     quando WorldModel::phase() == 3, com o dt do frame INTEIRO (dt*4).
//  3) Agent::updateData() TAMBEM chama controller() -- sobrescrito aqui como
//     no-op, senao a FSM avancaria mais de uma vez por frame.
//  4) 'myActor' (privado, em Agent) e' um safe_ptr -- REF-OWNING, nao um
//     ponteiro cru. initActor() aponta 'myActor' de volta para o PROPRIO
//     player que hospeda este agente (o container). Isso fecha um ciclo de
//     referencia: o player possui o agente via components:, e o agente
//     possui uma referencia de volta ao player via myActor. Sem quebrar
//     isso, nenhum dos dois lados chega a refcount zero por unref() externo
//     -- Agent::deleteData() (que zera myActor) so roda quando o proprio
//     Agent e destruido, o que nunca acontece por causa do ciclo. Por isso
//     shutdownNotification() abaixo solta a referencia ANTES de delegar
//     para a base (mesmo fix ja aplicado em
//     models/players/air/A-4/include/xnative/FlightAgentTC.hpp).
//
// O ator e o player que CONTEM o agente (initActor sobe a cadeia de
// containers).
//------------------------------------------------------------------------------
class ParatrooperAgentTC final : public base::ubf::AgentTC
{
   DECLARE_SUBCLASS(ParatrooperAgentTC, base::ubf::AgentTC)

public:
   ParatrooperAgentTC();

   long getDecisionCount() const { return decisions.load(std::memory_order_relaxed); }

   // Armadilha 3: mata o caminho de background herdado de ubf::Agent.
   void updateData(const double dt = 0.0) override;

protected:
   void controller(const double dt = 0.0) override;
   void initActor() override;
   bool shutdownNotification() override;

private:
   std::atomic<long> decisions{};
};

} // namespace xparatrooper
} // namespace models
} // namespace mixr
