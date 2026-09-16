#pragma once

#include "mixr/base/ubf/Agent.hpp"

namespace mixr {
namespace models {
namespace xAAA {

//------------------------------------------------------------------------------
// Class: AaaAgent
//
// Description: Subclasse MINIMA de base::ubf::Agent (o agente NATIVO de
//              FUNDO, factory nativa "UbfAgent") -- existe SO' para fechar
//              um ciclo de referencia com o proprio player no shutdown (ver
//              abaixo). Nao muda NENHUM comportamento de decisao:
//              controller()/initActor()/updateData() continuam sendo os da
//              propria base::ubf::Agent, exatamente como o ( UbfAgent )
//              nativo que este modelo usava direto antes desta classe
//              existir. docs/ARCHITECTURE.md, secao "Por que NAO ha nenhuma
//              subclasse de AgentTC aqui", continua valendo -- esta classe
//              NAO decide em tempo critico, so' fecha o ciclo no shutdown.
//
// Factory name: AaaAgent
//
// Slots: nenhum proprio. 'state'/'behavior' sao herdados de ubf::Agent.
//
// A UNICA ARMADILHA QUE ESTA CLASSE RESOLVE
//
//  'myActor' (privado, em Agent) e' um safe_ptr -- REF-OWNING, nao um
//  ponteiro cru. Agent::initActor() (a versao NATIVA, nunca sobrescrita
//  aqui) ja aponta 'myActor' de volta para o proprio container() -- que,
//  declarado dentro de components: de AaaSite, e' a propria antiaerea.
//  Isso fecha um ciclo de referencia: o player possui o agente via
//  components:, e o agente possui uma referencia de volta ao player via
//  myActor. Sem quebrar isso, nenhum dos dois lados chega a refcount zero
//  por unref() externo -- Agent::deleteData() (que zera myActor) so roda
//  quando o proprio Agent e destruido, o que nunca acontece por causa do
//  ciclo. Mesmo bug ja corrigido em models/players/air/A-4 (e replicado em
//  C-130/paratrooper/Navstar-3) -- so' que ali a classe de agente ja
//  existia por outro motivo (AgentTC, decisao em tempo critico) e aqui
//  precisou nascer so' por causa deste fix.
//------------------------------------------------------------------------------
class AaaAgent final : public base::ubf::Agent
{
   DECLARE_SUBCLASS(AaaAgent, base::ubf::Agent)

public:
   AaaAgent();

protected:
   bool shutdownNotification() override;
};

} // namespace xAAA
} // namespace models
} // namespace mixr
