#ifndef __xnative_FlightAgentTC_H__
#define __xnative_FlightAgentTC_H__

#include "mixr/base/ubf/Agent.hpp"

#include <atomic>

namespace mixr {
namespace models { class AirVehicle; }

namespace xnative {

//------------------------------------------------------------------------------
// Class: FlightAgentTC
//
// Description: A UNICA classe que este subprojeto acrescenta em relacao ao
//              single-thread. Roda o ciclo do UBF (percepcao/decisao/
//              atuacao) na FASE 3 do frame de TEMPO CRITICO, para a
//              aeronave que a contem -- no lugar do models::SimAgent
//              nativo, que roda em updateData() (thread de background).
//
// Factory name: FlightAgentTC
//
// Slots: nenhum proprio. 'state' e 'behavior' sao herdados de ubf::Agent --
//        a busca de slot sobe a hierarquia, entao EMPTY_SLOTTABLE aqui nao
//        esconde os slots da base.
//
// TRES ARMADILHAS DO FRAMEWORK QUE ESTA CLASSE RESOLVE
//
//  1) 'UbfAgentTC' NAO E CONSTRUIDO POR NENHUMA FACTORY DO MIXR.
//     base/factory.cpp registra apenas 'UbfAgent' e 'UbfArbiter'. Escrever
//     ( UbfAgentTC ... ) no EDL nao constroi nada -- a menos que a
//     aplicacao registre a classe na propria factory, que e o que
//     xnative::factory() faz com esta subclasse. Nao ha atalho: um agente
//     de tempo critico e, na pratica, codigo da aplicacao.
//
//  2) AgentTC::updateTC() CHAMA controller() EM TODA FASE.
//     O agente e um base::Component dentro do player, e a lista de players
//     e percorrida 4x por frame (uma vez por fase, com dt/4). Sem filtro, a
//     decisao rodaria 4 vezes por frame -- e tres delas nas fases erradas
//     (dinamica, transmissao, recepcao). Aqui controller() so roda quando
//     WorldModel::phase() == 3, e com o dt do frame INTEIRO (dt*4), que e
//     exatamente o que models::System faz com as suas quatro fases.
//
//  3) Agent::updateData() TAMBEM CHAMA controller().
//     AgentTC acrescenta updateTC() mas NAO desliga o caminho de background
//     herdado de Agent -- e Player::updateData() propaga para a lista de
//     componentes. Resultado: um agente dentro do player decidiria DUAS
//     vezes por frame, uma no tempo critico e outra na thread de
//     background (o filtro da armadilha 2 nao pega esse caminho: ao fim do
//     tcFrame a fase corrente FICA em 3). Por isso updateData() e
//     sobrescrito aqui como no-op: a decisao pertence ao frame, e so a ele.
//
// O ator e o player que CONTEM o agente (initActor sobe a cadeia de
// containers), entao o mesmo bloco EDL serve para qualquer aeronave --
// diferente do SimAgent nativo, que mora na Station e amarra o ator por
// NOME (slot 'actorPlayerName').
//
// Os dois contadores existem para o status e para o dump de determinismo:
// 'decisions' e observavel e deterministico (uma decisao por frame);
// 'lastThreadTag' mostra que a decisao roda dentro do pool T/C -- e por
// depender do escalonador NAO entra em nenhuma comparacao de determinismo.
//------------------------------------------------------------------------------
class FlightAgentTC : public base::ubf::AgentTC
{
   DECLARE_SUBCLASS(FlightAgentTC, base::ubf::AgentTC)

public:
   FlightAgentTC();

   long getDecisionCount() const { return decisions.load(std::memory_order_relaxed); }
   int getLastThreadTag() const  { return lastThreadTag.load(std::memory_order_relaxed); }

   // Armadilha 3: mata o caminho de background herdado de ubf::Agent.
   void updateData(const double dt = 0.0) override;

protected:
   void controller(const double dt = 0.0) override;
   void initActor() override;

private:
   std::atomic<long> decisions{};
   std::atomic<int> lastThreadTag{-1};
};

//------------------------------------------------------------------------------
// O agente de uma aeronave.
//
// Diferente do ( SimAgent ) do single-thread -- componente da STATION,
// amarrado ao ator por NOME -- este e componente do PLAYER. Entao a busca e
// a mesma que o framework usa para os subsistemas: por TIPO, na lista de
// componentes.
//
// Devolve nullptr se a aeronave nao declarar um ( FlightAgentTC ) no .edl.
//------------------------------------------------------------------------------
const FlightAgentTC* findFlightAgent(const models::AirVehicle* air);

} // namespace xnative
} // namespace mixr

#endif
