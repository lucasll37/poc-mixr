#pragma once

#include "mixr/models/Actions.hpp"

#include "mixr/base/String.hpp"

namespace mixr {
namespace base { class Number; class Time; }
namespace models {
namespace xC_130 {

//------------------------------------------------------------------------------
// Class: ActionParatrooperStick
//
// Description: Libera VARIAS estacoes "PARATROOPER" ao longo do tempo --
//              'count' paraquedistas, um a cada 'interval' segundos --
//              a partir de um UNICO cruzamento de steerpoint. Termo real do
//              meio militar: um "stick" e' o grupo que salta em sequencia
//              rapida na MESMA passagem da aeronave, ao contrario de
//              ActionParatrooperRelease (esta classe irma, um disparo por
//              cruzamento -- reaproveitada aqui via wrap:true nos cenarios
//              mais simples).
//
// Factory name: C130ActionParatrooperStick
//
// Slots:
//    storeType <String> ! tipo da estacao a liberar (default: "PARATROOPER")
//    count     <Number> ! quantos liberar ao todo (default: 1)
//    interval  <Time>   ! intervalo entre uma liberacao e a proxima (default: 1.0 s)
//    interval  <Number> ! idem, em segundos
//
// COMO O TEMPO ENTRA -- sem inventar mecanismo nenhum de "ticking": o proprio
// mixr::models::Action ja tem exatamente o que se precisa aqui.
// OnboardComputer::triggerAction() chama trigger() UMA vez (no cruzamento do
// steerpoint); se, depois disso, action->isInProgress() continuar true (o
// 'manager' que Action::trigger() guarda, ainda nao nulo -- ver Actions.cpp),
// o OBC GUARDA a acao e chama action->process(dt) A CADA CICLO DE FUNDO
// (OnboardComputer::updateData() -> actionManager(), 10 Hz em tempo real, no
// MESMO passo que o tcFrame() em '-deterministic') ate' action->isCompleted()
// virar true. Esta classe usa esse mecanismo JA EXISTENTE: trigger() libera a
// PRIMEIRA estacao e permanece em progresso (nao chama setCompleted());
// process() acumula 'dt' e libera mais uma a cada 'interval_' segundos
// decorridos, ate 'count_' liberacoes -- entao completa.
//
// DETERMINISMO: process() roda no laco de FUNDO (thread unica, fora do pool
// de tempo critico) -- o mesmo raciocinio ja documentado para libs/xmsg no
// CLAUDE.md raiz ("o custo la e estruturalmente zero" quanto a threads T/C).
// O acumulador de tempo simulado (nao contagem de chamadas) e o que torna o
// resultado igual chamando a 10 Hz (tempo real) ou a 50 Hz
// ('-deterministic', que chama updateData() no MESMO passo do tcFrame) --
// mesma tolerancia de ponto flutuante ja documentada em libs/xmsg/
// rules/timeTolerance.hpp para o mesmo tipo de acumulo.
//
// DEGRADACAO: se releaseNextStoreOfType() falhar (acabou a carga, ou
// storeType_ nao bate com nenhuma estacao) durante process(), a sequencia
// completa IMEDIATAMENTE em vez de ficar "em progresso" para sempre esperando
// uma estacao que nunca vai aparecer.
//------------------------------------------------------------------------------
class ActionParatrooperStick final : public mixr::models::Action
{
   DECLARE_SUBCLASS(ActionParatrooperStick, mixr::models::Action)

public:
   ActionParatrooperStick();

   bool trigger(OnboardComputer* const mgr) override;
   void process(const double dt) override;

   int getReleaseCount() const   { return count_; }
   int getReleasedSoFar() const  { return released_; }

private:
   base::String storeType_{"PARATROOPER"};
   int count_{1};
   double intervalSec_{1.0};

   int released_{};
   double elapsed_{};

   // Devolve true so' se a liberacao desta chamada TEVE EFEITO (achou e
   // liberou uma estacao) -- e' o que faz trigger() devolver false quando
   // 'mgr' e' nulo ou nao ha estacao livre, em vez de sempre 'true' so por
   // count_ ser positivo.
   bool releaseOneAndAdvance();

   bool setSlotStoreType(const base::String* const);
   bool setSlotCount(const base::Number* const);
   bool setSlotInterval(const base::Time* const);
   bool setSlotInterval(const base::Number* const);
};

} // namespace xC_130
} // namespace models
} // namespace mixr
