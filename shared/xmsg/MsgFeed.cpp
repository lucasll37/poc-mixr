#include "xmsg/MsgFeed.hpp"

#include "mixr/base/Identifier.hpp"
#include "mixr/base/Pair.hpp"
#include "mixr/base/PairStream.hpp"
#include "mixr/base/String.hpp"
#include "mixr/base/numeric/Number.hpp"

#include "mixr/models/player/Player.hpp"
#include "mixr/simulation/Simulation.hpp"
#include "mixr/simulation/Station.hpp"

#include "xmsg/MsgReport.hpp"
#include "xmsg/MsgSink.hpp"
#include "xmsg/SlotUnits.hpp"
#include "xmsg/SnapshotSource.hpp"
#include "xmsg/rules/timeTolerance.hpp"
#include "xlog/Log.hpp"

namespace mixr {
namespace xmsg {

IMPLEMENT_SUBCLASS(MsgFeed, "MsgFeed")

// clang-format off
BEGIN_SLOTTABLE(MsgFeed)
   "trackManager", "maxPlayers", "healthEvery", "sinks", "messages",
END_SLOTTABLE(MsgFeed)

BEGIN_SLOT_MAP(MsgFeed)
   ON_SLOT(1, setSlotTrackManager,    base::Identifier)
   ON_SLOT(1, setSlotTrackManagerStr, base::String)
   ON_SLOT(2, setSlotMaxPlayers,      base::Number)
   ON_SLOT(3, setSlotHealthEvery,     base::Number)
   ON_SLOT(4, setSlotSinks,           base::PairStream)
   ON_SLOT(5, setSlotMessages,        base::PairStream)
END_SLOT_MAP()

MsgFeed::MsgFeed()
// clang-format on
{
   STANDARD_CONSTRUCTOR()
}

void MsgFeed::copyData(const MsgFeed& org, const bool)
{
   BaseClass::copyData(org);
   trackManagerName_ = org.trackManagerName_;
   maxPlayers_ = org.maxPlayers_;
   healthEvery_ = org.healthEvery_;

   if (sinksSlot_ != nullptr) { sinksSlot_->unref(); sinksSlot_ = nullptr; }
   if (org.sinksSlot_ != nullptr) { sinksSlot_ = org.sinksSlot_; sinksSlot_->ref(); }

   if (messagesSlot_ != nullptr) { messagesSlot_->unref(); messagesSlot_ = nullptr; }
   if (org.messagesSlot_ != nullptr) { messagesSlot_ = org.messagesSlot_; messagesSlot_->ref(); }

   sinks_.clear();
   messages_.clear();
   playerSlots_.clear();
   nextSlot_ = 0;
   ready_ = false;
}

void MsgFeed::deleteData()
{
   for (auto* s : sinks_) s->close();
   if (sinksSlot_ != nullptr)    { sinksSlot_->unref();    sinksSlot_ = nullptr; }
   if (messagesSlot_ != nullptr) { messagesSlot_->unref(); messagesSlot_ = nullptr; }
}

bool MsgFeed::setSlotTrackManager(const base::Identifier* const x)
{
   if (x == nullptr || x->getString() == nullptr) return false;
   trackManagerName_ = x->getString();
   return true;
}

bool MsgFeed::setSlotTrackManagerStr(const base::String* const x)
{
   if (x == nullptr || x->getString() == nullptr) return false;
   trackManagerName_ = x->getString();
   return true;
}

bool MsgFeed::setSlotMaxPlayers(const base::Number* const x)
{
   if (x == nullptr) return false;
   maxPlayers_ = x->getInt();
   return maxPlayers_ > 0;
}

bool MsgFeed::setSlotHealthEvery(const base::Number* const x)
{
   return captureSeconds(x, healthEvery_);
}

bool MsgFeed::setSlotSinks(base::PairStream* const x)
{
   if (sinksSlot_ != nullptr) { sinksSlot_->unref(); sinksSlot_ = nullptr; }
   if (x == nullptr) return false;
   sinksSlot_ = x;
   sinksSlot_->ref();
   return true;
}

bool MsgFeed::setSlotMessages(base::PairStream* const x)
{
   if (messagesSlot_ != nullptr) { messagesSlot_->unref(); messagesSlot_ = nullptr; }
   if (x == nullptr) return false;
   messagesSlot_ = x;
   messagesSlot_->ref();
   return true;
}

//------------------------------------------------------------------------------
// reset() -- resolve TUDO aqui: nomes de campo viram indices, o estado por
// player e dimensionado, os arquivos abrem. Depois disso o caminho quente nao
// aloca e nao faz busca por nome.
//------------------------------------------------------------------------------
void MsgFeed::reset()
{
   BaseClass::reset();

   sinks_.clear();
   messages_.clear();
   playerSlots_.clear();
   nextSlot_ = 0;
   overCap_ = 0;
   emitted_ = 0;
   overflows_ = 0;
   sinceHealth_ = 0.0;
   ready_ = false;

   if (sinksSlot_ != nullptr) {
      base::List::Item* item{sinksSlot_->getFirstItem()};
      while (item != nullptr) {
         const auto pair = static_cast<base::Pair*>(item->getValue());
         const auto sink = (pair != nullptr) ? dynamic_cast<MsgSink*>(pair->object()) : nullptr;
         if (sink == nullptr) {
            LOG(ERROR) << "[xmsg] item de 'sinks:' nao e um destino de mensagem";
            return;
         }
         if (!sink->open()) return;      // open() ja logou a causa
         sinks_.push_back(sink);
         item = item->getNext();
      }
   }
   if (sinks_.empty()) {
      LOG(WARNING) << "[xmsg] nenhum 'sinks:' declarado -- as mensagens nao vao a lugar nenhum";
   }

   if (messagesSlot_ != nullptr) {
      base::List::Item* item{messagesSlot_->getFirstItem()};
      while (item != nullptr) {
         const auto pair = static_cast<base::Pair*>(item->getValue());
         const auto msg = (pair != nullptr) ? dynamic_cast<MsgReport*>(pair->object()) : nullptr;
         if (msg == nullptr) {
            LOG(ERROR) << "[xmsg] item de 'messages:' nao e uma MsgReport";
            return;
         }
         if (!msg->prepare(maxPlayers_)) return;
         messages_.push_back(msg);
         item = item->getNext();
      }
   }
   if (messages_.empty()) {
      LOG(WARNING) << "[xmsg] nenhuma mensagem declarada em 'messages:'";
      return;
   }

   ready_ = true;
}

bool MsgFeed::shutdownNotification()
{
   // Os sinks estao em SLOT, nao em components: -- a cascata nativa de
   // shutdown nao chega ate eles, entao ela e feita a mao. E o preco de
   // manter Component::updateTC() sem caminho ate aqui (ver o .hpp).
   for (auto* s : sinks_) s->close();
   return BaseClass::shutdownNotification();
}

simulation::Station* MsgFeed::findStation()
{
   // O 'components:' da Station encadeia container() nos filhos
   // (Component::processComponents), ao contrario do slot 'outputHandler:' do
   // dataRecorder -- que e a armadilha 7 do xtacview. Aqui a subida funciona.
   return dynamic_cast<simulation::Station*>(
      findContainerByType(typeid(simulation::Station)));
}

//------------------------------------------------------------------------------
// slotFor() -- indice estavel por NOME de player.
//
// Por nome, e nao por getID(): o id de um player que chega por DIS vem do fio
// e pode colidir entre federados. O teto evita crescer sem fim numa federacao
// movimentada -- acima dele o player e ignorado e a contagem vai para a
// mensagem de saude, em vez de o sistema silenciar sem dizer por que.
//------------------------------------------------------------------------------
int MsgFeed::slotFor(const std::string& playerName)
{
   const auto it = playerSlots_.find(playerName);
   if (it != playerSlots_.end()) return it->second;

   if (nextSlot_ >= maxPlayers_) { ++overCap_; return -1; }

   const int slot{nextSlot_++};
   playerSlots_[playerName] = slot;
   return slot;
}

void MsgFeed::dispatch(const std::string& msgName)
{
   if (writer_.overflow()) { ++overflows_; return; }

   for (auto* s : sinks_) {
      if (s->accepts(msgName)) s->write(writer_.data(), writer_.size());
   }
   ++emitted_;
}

void MsgFeed::emitHealth(const double t)
{
   long deferred{};
   for (const auto* m : messages_) deferred += m->deferred();

   long failed{};
   for (const auto* s : sinks_) failed += s->failed();

   writer_.begin(t, "msgHealth");
   writer_.addInt("emitted", emitted_);
   writer_.addInt("suppressed", deferred);
   writer_.addInt("states", static_cast<long>(playerSlots_.size()));
   writer_.addInt("overCap", overCap_);
   writer_.addInt("sendFailed", failed);
   writer_.addInt("overflow", overflows_);
   writer_.end();

   // A saude vai para TODOS os sinks, sem passar pelo filtro por assinante:
   // um destino que nao soubesse que houve supressao mentiria por omissao.
   for (auto* s : sinks_) s->write(writer_.data(), writer_.size());
}

void MsgFeed::updateData(const double dt)
{
   BaseClass::updateData(dt);
   if (!ready_) return;

   simulation::Station* const station{findStation()};
   if (station == nullptr) return;

   simulation::Simulation* const sim{station->getSimulation()};
   if (sim == nullptr) return;

   const double t{sim->getExecTimeSec()};

   // Pre-ref'd: tem de dar unref(). E varrido a CADA ciclo, nunca cacheado --
   // e o que faz um player que chega por DIS entrar nas mensagens sozinho.
   base::PairStream* const players{sim->getPlayers()};
   if (players == nullptr) return;

   base::List::Item* item{players->getFirstItem()};
   while (item != nullptr) {
      const auto pair = static_cast<base::Pair*>(item->getValue());
      item = item->getNext();
      if (pair == nullptr) continue;

      const auto player = dynamic_cast<models::Player*>(pair->object());
      if (player == nullptr) continue;

      const char* const nome{(player->getName() != nullptr)
                              ? player->getName()->getString() : nullptr};
      if (nome == nullptr) continue;

      // Uniao dos grupos de quem quer este player: a amostragem acontece UMA
      // vez por player, e so nos grupos pedidos. Quem nao pede motor nao paga
      // a descida no FGPropulsion.
      unsigned mask{};
      for (const auto* m : messages_) {
         if (m->wantsPlayer(nome)) mask |= m->groupMask();
      }
      if (mask == 0) continue;

      const int slot{slotFor(nome)};
      if (slot < 0) continue;

      fillSnapshot(snap_, player, mask, trackManagerName_);

      for (auto* m : messages_) {
         if (!m->wantsPlayer(nome)) continue;
         if (!m->evaluate(dt, snap_, slot)) continue;

         m->render(writer_, t, snap_);
         dispatch(m->name());
      }
   }
   players->unref();

   sinceHealth_ += dt;
   if (healthEvery_ > 0.0 && rules::reached(sinceHealth_, healthEvery_)) {
      emitHealth(t);
      sinceHealth_ = 0.0;
   }

   for (auto* s : sinks_) s->tick(dt);
}

} // namespace xmsg
} // namespace mixr
