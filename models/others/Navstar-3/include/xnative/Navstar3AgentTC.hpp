#pragma once

#include "mixr/base/ubf/Agent.hpp"

#include <atomic>

namespace mixr {
namespace models {
namespace xNavstar_3 {

//------------------------------------------------------------------------------
// Class: Navstar3AgentTC
//
// Description: O UNICO agente deste modelo. Roda o ciclo do UBF (percepcao/
//              decisao/atuacao) na FASE 3 do frame de TEMPO CRITICO, para o
//              player que o contem. Adaptado (nao linkado) de
//              models/players/A-4/include/xnative/FlightAgentTC.hpp,
//              models/players/C-130/include/xnative/FlightAgentTC.hpp e
//              models/players/paratrooper/include/xnative/ParatrooperAgentTC.hpp.
//
// Factory name: Navstar3AgentTC
//
// Slots: nenhum proprio. 'state'/'behavior' sao herdados de ubf::Agent.
//
// TRES ARMADILHAS DO FRAMEWORK QUE ESTA CLASSE RESOLVE (as mesmas de todo
// agente deste repositorio, resolvidas identicamente aqui):
//
//  1) 'AgentTC' NAO E CONSTRUIDO POR NENHUMA FACTORY DO MIXR -- registro
//     manual em xnative::factory().
//  2) AgentTC::updateTC() chama controller() em TODA fase -- aqui so roda
//     quando WorldModel::phase() == 3, com o dt do frame INTEIRO (dt*4).
//  3) Agent::updateData() TAMBEM chama controller() -- sobrescrito aqui como
//     no-op, senao a orbita avancaria mais de uma vez por frame.
//
// O ator e o player que CONTEM o agente (initActor sobe a cadeia de
// containers).
//------------------------------------------------------------------------------
class Navstar3AgentTC final : public base::ubf::AgentTC
{
   DECLARE_SUBCLASS(Navstar3AgentTC, base::ubf::AgentTC)

public:
   Navstar3AgentTC();

   long getDecisionCount() const { return decisions.load(std::memory_order_relaxed); }

   // Armadilha 3: mata o caminho de background herdado de ubf::Agent.
   void updateData(const double dt = 0.0) override;

protected:
   void controller(const double dt = 0.0) override;
   void initActor() override;

private:
   std::atomic<long> decisions{};
};

} // namespace xNavstar_3
} // namespace models
} // namespace mixr
