#include "xmsg/MsgReport.hpp"

#include "mixr/base/Identifier.hpp"
#include "mixr/base/Pair.hpp"
#include "mixr/base/PairStream.hpp"
#include "mixr/base/String.hpp"
#include "mixr/base/numeric/Number.hpp"

#include "xmsg/Condition.hpp"
#include "xmsg/FieldCatalog.hpp"
#include "xmsg/RecordWriter.hpp"
#include "xmsg/SlotUnits.hpp"
#include "xmsg/SnapshotSource.hpp"
#include "xlog/Log.hpp"

#include <cstring>

namespace mixr {
namespace xmsg {

IMPLEMENT_SUBCLASS(MsgReport, "MsgReport")

// clang-format off
BEGIN_SLOTTABLE(MsgReport)
   "name", "players", "labels", "fields", "when", "match", "every",
END_SLOTTABLE(MsgReport)

BEGIN_SLOT_MAP(MsgReport)
   ON_SLOT(1, setSlotName,    base::String)
   ON_SLOT(1, setSlotNameId,  base::Identifier)
   ON_SLOT(2, setSlotPlayers, base::PairStream)
   ON_SLOT(3, setSlotLabels,  base::PairStream)
   ON_SLOT(4, setSlotFields,  base::PairStream)
   ON_SLOT(5, setSlotWhen,    base::PairStream)
   ON_SLOT(6, setSlotMatch,   base::Identifier)
   ON_SLOT(7, setSlotEvery,   base::Number)
END_SLOT_MAP()

MsgReport::MsgReport()
// clang-format on
{
   STANDARD_CONSTRUCTOR()
}

void MsgReport::copyData(const MsgReport& org, const bool)
{
   BaseClass::copyData(org);
   name_ = org.name_;
   players_ = org.players_;
   labels_ = org.labels_;
   fieldNames_ = org.fieldNames_;
   matchAll_ = org.matchAll_;
   every_ = org.every_;

   if (when_ != nullptr) { when_->unref(); when_ = nullptr; }
   if (org.when_ != nullptr) { when_ = org.when_; when_->ref(); }

   fields_.clear();
   conds_.clear();
   gates_.clear();
   groupMask_ = 0;
}

void MsgReport::deleteData()
{
   if (when_ != nullptr) { when_->unref(); when_ = nullptr; }
}

namespace {

//------------------------------------------------------------------------------
// Leitura de listas de NOMES do EDL -- e ha DUAS formas, com o nome em lugares
// opostos. Descoberto quebrando: 'fields: { latDeg lonDeg }' chegava aqui como
// os campos "1" e "2".
//
//    { latDeg lonDeg }        itens ANONIMOS: o parser numera os slots ("1",
//                             "2", ...) e o nome de verdade vai no OBJETO,
//                             como base::Identifier (edl_parser.y:144-155).
//    { latDeg: x  lonDeg: y } forma chave:valor: ai sim o nome esta no SLOT.
//
// Ler o objeto primeiro e cair para o slot cobre as duas, e evita a armadilha
// irma: um valor sem aspas e base::Identifier, nao base::String -- um
// dynamic_cast<String> sozinho falharia em silencio.
//------------------------------------------------------------------------------
void readNames(const base::PairStream* const x, std::vector<std::string>& out)
{
   out.clear();
   if (x == nullptr) return;

   const base::List::Item* item{x->getFirstItem()};
   while (item != nullptr) {
      const auto pair = static_cast<const base::Pair*>(item->getValue());
      item = item->getNext();
      if (pair == nullptr) continue;

      const base::Object* const obj{pair->object()};
      if (const auto id = dynamic_cast<const base::Identifier*>(obj)) {
         if (id->getString() != nullptr) { out.emplace_back(id->getString()); continue; }
      }
      if (const auto str = dynamic_cast<const base::String*>(obj)) {
         if (str->getString() != nullptr) { out.emplace_back(str->getString()); continue; }
      }
      if (pair->slot() != nullptr) out.emplace_back(pair->slot()->getString());
   }
}

} // namespace

bool MsgReport::setSlotName(const base::String* const x)
{
   if (x == nullptr || x->getString() == nullptr) return false;
   name_ = x->getString();
   return !name_.empty();
}

bool MsgReport::setSlotNameId(const base::Identifier* const x)
{
   if (x == nullptr || x->getString() == nullptr) return false;
   name_ = x->getString();
   return !name_.empty();
}

bool MsgReport::setSlotPlayers(const base::PairStream* const x)
{
   readNames(x, players_);
   return true;
}

bool MsgReport::setSlotLabels(const base::PairStream* const x)
{
   readNames(x, labels_);
   return true;
}

bool MsgReport::setSlotFields(const base::PairStream* const x)
{
   readNames(x, fieldNames_);
   return true;
}

bool MsgReport::setSlotWhen(base::PairStream* const x)
{
   if (when_ != nullptr) { when_->unref(); when_ = nullptr; }
   if (x == nullptr) return false;
   when_ = x;
   when_->ref();
   return true;
}

bool MsgReport::setSlotMatch(const base::Identifier* const x)
{
   if (x == nullptr || x->getString() == nullptr) return false;
   const std::string v{x->getString()};
   if (v == "all") { matchAll_ = true;  return true; }
   if (v == "any") { matchAll_ = false; return true; }

   LOG(ERROR) << "[xmsg] 'match:' aceita 'any' ou 'all', recebeu '" << v << "'";
   return false;
}

bool MsgReport::setSlotEvery(const base::Number* const x)
{
   return captureSeconds(x, every_);
}

bool MsgReport::prepare(const int maxPlayers)
{
   if (name_.empty()) {
      LOG(ERROR) << "[xmsg] MsgReport sem slot 'name:'";
      return false;
   }
   if (fieldNames_.empty()) {
      LOG(ERROR) << "[xmsg] mensagem '" << name_ << "' sem slot 'fields:'";
      return false;
   }

   fields_.clear();
   groupMask_ = 0;
   for (const auto& fn : fieldNames_) {
      const FieldInfo* const info{findField(fn)};
      if (info == nullptr) {
         LOG(ERROR) << "[xmsg] mensagem '" << name_ << "': campo desconhecido '"
                    << fn << "' -- validos: " << allFieldNames();
         return false;
      }
      fields_.push_back(info->index);
      groupMask_ |= groupBit(info->group);
   }

   // Os rotulos tambem custam grupo: 'mode' e 'side' vem do grupo Ident, e
   // 'track' obriga a varrer o track manager.
   for (const auto& l : labels_) {
      if (l == "side" || l == "mode" || l == "player") groupMask_ |= groupBit(Group::Ident);
      else if (l == "track") groupMask_ |= groupBit(Group::Track);
      else {
         LOG(ERROR) << "[xmsg] mensagem '" << name_ << "': rotulo desconhecido '"
                    << l << "' -- validos: player side mode track";
         return false;
      }
   }

   conds_.clear();
   if (when_ != nullptr) {
      base::List::Item* item{when_->getFirstItem()};
      while (item != nullptr) {
         const auto pair = static_cast<base::Pair*>(item->getValue());
         const auto cond = (pair != nullptr) ? dynamic_cast<Condition*>(pair->object()) : nullptr;
         if (cond == nullptr) {
            LOG(ERROR) << "[xmsg] mensagem '" << name_
                       << "': item de 'when:' nao e uma condicao";
            return false;
         }
         if (!cond->prepare(maxPlayers)) return false;

         // A condicao le um campo que a amostragem precisa ter preenchido --
         // senao ela avaliaria sempre sobre grupo invalido e nunca dispararia.
         groupMask_ |= groupBit(cond->group());
         conds_.push_back(cond);
         item = item->getNext();
      }
   }

   rules::EmitGate modelo;
   modelo.configure(every_);
   gates_.assign(static_cast<std::size_t>(maxPlayers > 0 ? maxPlayers : 0), modelo);
   return true;
}

bool MsgReport::wantsPlayer(const char* const playerName) const
{
   if (players_.empty()) return true;
   if (playerName == nullptr) return false;
   for (const auto& p : players_) {
      if (p == playerName) return true;
   }
   return false;
}

bool MsgReport::evaluate(const double dt, const Snapshot& snap, const int slot)
{
   if (slot < 0 || slot >= static_cast<int>(gates_.size())) return false;

   bool quer{};
   if (conds_.empty()) {
      quer = true;                              // periodica
   } else if (matchAll_) {
      // 'all' com bordas: exige que TODAS disparem no mesmo ciclo. E restritivo
      // de proposito -- 'all' sobre niveis exigiria guardar o nivel de cada
      // condicao, e ai 'any' e 'all' teriam semanticas incomparaveis.
      quer = true;
      for (auto* c : conds_) {
         if (!c->evaluate(dt, snap, slot)) quer = false;
      }
   } else {
      // Avalia TODAS mesmo que uma ja tenha disparado: as condicoes tem estado
      // (histerese, janela, ultimo emitido) e pular uma a dessincronizaria.
      for (auto* c : conds_) {
         if (c->evaluate(dt, snap, slot)) quer = true;
      }
   }

   return gates_[static_cast<std::size_t>(slot)].update(dt, quer);
}

void MsgReport::render(RecordWriter& w, const double t, const Snapshot& snap) const
{
   w.begin(t, name_.c_str());

   for (const auto& l : labels_) {
      if (l == "player")     w.addLabel("player", snap.playerName);
      else if (l == "side")  w.addLabel("side", snap.sideName);
      else if (l == "mode")  w.addLabel("mode", snap.modeName);
      else if (l == "track") w.addLabel("track", snap.trackName);
   }

   for (const int idx : fields_) {
      const FieldInfo* const info{fieldByIndex(idx)};
      if (info == nullptr) continue;
      w.addField(*info, snap.v[idx], snap.valid(info->group));
   }

   w.end();
}

void MsgReport::resetSlot(const int slot)
{
   if (slot >= 0 && slot < static_cast<int>(gates_.size())) {
      gates_[static_cast<std::size_t>(slot)].reset();
   }
   for (auto* c : conds_) c->resetSlot(slot);
}

//------------------------------------------------------------------------------
// deferred() -- so conta para mensagem de EVENTO.
//
// Numa mensagem periodica o 'every:' e um limitador de taxa, e nao emitir a
// cada ciclo E o comportamento pedido: nada foi perdido nem atrasado, a
// proxima amostra vem logo e e igualmente boa. Contar isso como "suprimido"
// enchia a mensagem de saude de milhares por minuto (medido: 4900 em 24 s
// simulados) e afogava o numero que importa -- a borda de evento que teve de
// esperar o piso vencer.
//------------------------------------------------------------------------------
long MsgReport::deferred() const
{
   if (conds_.empty()) return 0;

   long total{};
   for (const auto& g : gates_) total += g.deferred();
   return total;
}

} // namespace xmsg
} // namespace mixr
