#include "xnative/ActionParatrooperStick.hpp"

#include "xnative/ReleaseHelpers.hpp"

#include "mixr/base/numeric/Number.hpp"
#include "mixr/base/units/Times.hpp"

namespace mixr {
namespace models {
namespace xC_130 {

IMPLEMENT_SUBCLASS(ActionParatrooperStick, "C130ActionParatrooperStick")
EMPTY_DELETEDATA(ActionParatrooperStick)

// clang-format off
BEGIN_SLOTTABLE(ActionParatrooperStick)
   "storeType",   // 1
   "count",       // 2
   "interval",    // 3
END_SLOTTABLE(ActionParatrooperStick)

BEGIN_SLOT_MAP(ActionParatrooperStick)
   ON_SLOT(1, setSlotStoreType, base::String)
   ON_SLOT(2, setSlotCount,     base::Number)
   ON_SLOT(3, setSlotInterval,  base::Time)
   ON_SLOT(3, setSlotInterval,  base::Number)
END_SLOT_MAP()

ActionParatrooperStick::ActionParatrooperStick()
// clang-format on
{
   STANDARD_CONSTRUCTOR()
}

void ActionParatrooperStick::copyData(const ActionParatrooperStick& org, const bool)
{
   BaseClass::copyData(org);
   storeType_ = org.storeType_;
   count_ = org.count_;
   intervalSec_ = org.intervalSec_;
   released_ = org.released_;
   elapsed_ = org.elapsed_;
}

//------------------------------------------------------------------------------
// releaseOneAndAdvance() -- o unico ponto que chama releaseNextStoreOfType()
// e mexe em 'released_'. Se a carga acabou (ou storeType_ nao bate com
// estacao nenhuma), completa a sequencia na hora -- nao fica "em progresso"
// esperando uma estacao que nunca vai aparecer. Devolve se ESTA chamada
// liberou de verdade -- e' o que 'trigger()' repassa como seu proprio
// retorno (ao inves de so' refletir 'count_ > 0').
//------------------------------------------------------------------------------
bool ActionParatrooperStick::releaseOneAndAdvance()
{
   const bool released{releaseNextStoreOfType(getManager(), storeType_)};

   if (released) {
      ++released_;
   } else {
      released_ = count_;   // forca o fim da sequencia -- ver setCompleted() abaixo
   }

   if (released_ >= count_) setCompleted(true);

   return released;
}

//------------------------------------------------------------------------------
// trigger() -- chamado UMA vez por OnboardComputer::triggerAction(), no
// cruzamento do steerpoint. Libera a PRIMEIRA estacao na hora (um "stick" de
// verdade comeca a saltar assim que autorizado, nao 'interval_' segundos
// depois) e, se sobrar mais gente pra liberar, permanece em progresso --
// BaseClass::trigger() ja fez 'completed = false' e guardou 'mgr'; NAO
// chamamos setCompleted(true) aqui como ActionParatrooperRelease faz, a
// menos que 'count_' ja nasca zerado ou a PRIMEIRA liberacao falhe.
//------------------------------------------------------------------------------
bool ActionParatrooperStick::trigger(OnboardComputer* const mgr)
{
   BaseClass::trigger(mgr);

   released_ = 0;
   elapsed_ = 0.0;

   if (count_ <= 0) {
      setCompleted(true);
      return false;
   }

   return releaseOneAndAdvance();
}

//------------------------------------------------------------------------------
// process() -- chamado a cada ciclo de FUNDO enquanto isInProgress() (ver
// OnboardComputer::actionManager()). Acumula tempo SIMULADO (nao conta
// chamadas) -- o mesmo valor sai identico seja 'process()' chamado a 10 Hz
// (tempo real) ou no mesmo passo do tcFrame de '-deterministic'.
//------------------------------------------------------------------------------
void ActionParatrooperStick::process(const double dt)
{
   BaseClass::process(dt);

   if (isCompleted()) return;

   elapsed_ += dt;
   while (elapsed_ >= intervalSec_ && !isCompleted()) {
      elapsed_ -= intervalSec_;
      releaseOneAndAdvance();
   }
}

bool ActionParatrooperStick::setSlotStoreType(const base::String* const msg)
{
   if (msg == nullptr) return false;
   storeType_ = *msg;
   return true;
}

bool ActionParatrooperStick::setSlotCount(const base::Number* const msg)
{
   if (msg == nullptr) return false;
   count_ = msg->getInt();
   return true;
}

bool ActionParatrooperStick::setSlotInterval(const base::Time* const msg)
{
   if (msg == nullptr) return false;
   intervalSec_ = base::Seconds::convertStatic(*msg);
   return true;
}

bool ActionParatrooperStick::setSlotInterval(const base::Number* const msg)
{
   if (msg == nullptr) return false;
   intervalSec_ = msg->getReal();
   return true;
}

} // namespace xC_130
} // namespace models
} // namespace mixr
