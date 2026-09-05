#include "xmsg/Condition.hpp"

#include "mixr/base/String.hpp"

#include "xlog/Log.hpp"

namespace mixr {
namespace xmsg {

IMPLEMENT_ABSTRACT_SUBCLASS(Condition, "XmsgCondition")

// clang-format off
BEGIN_SLOTTABLE(Condition)
   "field",
END_SLOTTABLE(Condition)

BEGIN_SLOT_MAP(Condition)
   ON_SLOT(1, setSlotField, base::String)
END_SLOT_MAP()

EMPTY_DELETEDATA(Condition)

Condition::Condition()
// clang-format on
{
   STANDARD_CONSTRUCTOR()
}

void Condition::copyData(const Condition& org, const bool)
{
   BaseClass::copyData(org);
   fieldName_ = org.fieldName_;
   index_ = org.index_;
   group_ = org.group_;
   dim_ = org.dim_;
   slots_ = org.slots_;
}

bool Condition::setSlotField(const base::String* const msg)
{
   if (msg == nullptr || msg->getString() == nullptr) return false;
   fieldName_ = msg->getString();
   return !fieldName_.empty();
}

//------------------------------------------------------------------------------
// prepare() -- nome de campo desconhecido e ERRO ALTO.
//
// A alternativa (ignorar em silencio) e exatamente a familia de armadilhas que
// este repositorio ja catalogou quatro vezes: factory nao encadeada,
// dataLogTime zero, valor sem aspas virando Identifier, comentario acentuado
// no .edl. Todas custaram tempo porque o sintoma era "nao acontece nada".
//------------------------------------------------------------------------------
bool Condition::prepare(const int maxPlayers)
{
   slots_ = (maxPlayers > 0) ? maxPlayers : 0;

   if (fieldName_.empty()) {
      LOG(ERROR) << "[xmsg] condicao sem slot 'field:'";
      return false;
   }

   const FieldInfo* const info{findField(fieldName_)};
   if (info == nullptr) {
      LOG(ERROR) << "[xmsg] campo desconhecido em 'field:': '" << fieldName_
                 << "' -- validos: " << allFieldNames();
      return false;
   }

   index_ = info->index;
   group_ = info->group;
   dim_ = info->dim;
   return true;
}

double Condition::read(const Snapshot& snap, bool& valid) const
{
   if (index_ < 0 || !snap.valid(group_)) { valid = false; return 0.0; }
   valid = true;
   return snap.v[index_];
}

} // namespace xmsg
} // namespace mixr
