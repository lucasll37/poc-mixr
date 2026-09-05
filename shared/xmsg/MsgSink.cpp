#include "xmsg/MsgSink.hpp"

#include "mixr/base/Identifier.hpp"
#include "mixr/base/Pair.hpp"
#include "mixr/base/PairStream.hpp"
#include "mixr/base/String.hpp"

namespace mixr {
namespace xmsg {

IMPLEMENT_ABSTRACT_SUBCLASS(MsgSink, "XmsgSink")

// clang-format off
BEGIN_SLOTTABLE(MsgSink)
   "messages",
END_SLOTTABLE(MsgSink)

BEGIN_SLOT_MAP(MsgSink)
   ON_SLOT(1, setSlotMessages, base::PairStream)
END_SLOT_MAP()

EMPTY_DELETEDATA(MsgSink)

MsgSink::MsgSink()
// clang-format on
{
   STANDARD_CONSTRUCTOR()
}

void MsgSink::copyData(const MsgSink& org, const bool)
{
   BaseClass::copyData(org);
   accept_ = org.accept_;
   failed_ = 0;
}

//------------------------------------------------------------------------------
// 'messages: { telemetria falha-motor }' -- uma lista de nomes.
//
// Mesma armadilha de MsgReport::readNames(): num '{ a b c }' os itens sao
// ANONIMOS -- o parser numera os slots ("1", "2", ...) e o nome de verdade vai
// no OBJETO, como base::Identifier. Ler o objeto primeiro e cair para o slot
// cobre tambem a forma 'chave: valor'.
//------------------------------------------------------------------------------
bool MsgSink::setSlotMessages(const base::PairStream* const x)
{
   if (x == nullptr) return false;

   accept_.clear();
   const base::List::Item* item{x->getFirstItem()};
   while (item != nullptr) {
      const auto pair = static_cast<const base::Pair*>(item->getValue());
      item = item->getNext();
      if (pair == nullptr) continue;

      const base::Object* const obj{pair->object()};
      if (const auto id = dynamic_cast<const base::Identifier*>(obj)) {
         if (id->getString() != nullptr) { accept_.emplace_back(id->getString()); continue; }
      }
      if (const auto str = dynamic_cast<const base::String*>(obj)) {
         if (str->getString() != nullptr) { accept_.emplace_back(str->getString()); continue; }
      }
      if (pair->slot() != nullptr) accept_.emplace_back(pair->slot()->getString());
   }
   return true;
}

bool MsgSink::accepts(const std::string& msgName) const
{
   if (accept_.empty()) return true;      // vazio = todas
   for (const auto& n : accept_) {
      if (n == msgName) return true;
   }
   return false;
}

} // namespace xmsg
} // namespace mixr
