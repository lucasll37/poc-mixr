#include "xmsg/MsgFileSink.hpp"

#include "mixr/base/String.hpp"
#include "mixr/base/numeric/Number.hpp"

#include "xmsg/SlotUnits.hpp"
#include "xmsg/rules/timeTolerance.hpp"
#include "xlog/Log.hpp"

namespace mixr {
namespace xmsg {

IMPLEMENT_SUBCLASS(MsgFileSink, "MsgFileSink")

// clang-format off
BEGIN_SLOTTABLE(MsgFileSink)
   "fileName", "flushEvery",
END_SLOTTABLE(MsgFileSink)

BEGIN_SLOT_MAP(MsgFileSink)
   ON_SLOT(1, setSlotFileName,   base::String)
   ON_SLOT(2, setSlotFlushEvery, base::Number)
END_SLOT_MAP()

MsgFileSink::MsgFileSink()
// clang-format on
{
   STANDARD_CONSTRUCTOR()
}

void MsgFileSink::copyData(const MsgFileSink& org, const bool)
{
   BaseClass::copyData(org);
   fileName_ = org.fileName_;
   flushEvery_ = org.flushEvery_;
   // O arquivo NAO e copiado: cada instancia abre o seu, preguicosamente.
   sinceFlush_ = 0.0;
   opened_ = false;
}

void MsgFileSink::deleteData()
{
   close();
}

bool MsgFileSink::setSlotFileName(const base::String* const x)
{
   if (x == nullptr || x->getString() == nullptr) return false;
   fileName_ = x->getString();
   return !fileName_.empty();
}

bool MsgFileSink::setSlotFlushEvery(const base::Number* const x)
{
   return captureSeconds(x, flushEvery_);
}

bool MsgFileSink::open()
{
   if (opened_) return true;

   if (fileName_.empty()) {
      LOG(ERROR) << "[xmsg] MsgFileSink sem slot 'fileName:'";
      return false;
   }

   out_.open(fileName_, std::ios::out | std::ios::trunc);
   if (!out_.is_open()) {
      LOG(ERROR) << "[xmsg] falha ao abrir '" << fileName_
                 << "' para gravacao -- o diretorio existe?";
      return false;
   }

   opened_ = true;
   sinceFlush_ = 0.0;
   return true;
}

void MsgFileSink::write(const char* const line, const std::size_t len)
{
   if (!opened_ || line == nullptr) { countFailure(); return; }

   out_.write(line, static_cast<std::streamsize>(len));
   out_.put('\n');
   if (!out_) countFailure();
}

void MsgFileSink::tick(const double dt)
{
   if (!opened_) return;

   sinceFlush_ += dt;
   if (rules::reached(sinceFlush_, flushEvery_)) {
      out_.flush();
      sinceFlush_ = 0.0;
   }
}

void MsgFileSink::close()
{
   if (opened_) {
      out_.flush();
      out_.close();
      opened_ = false;
   }
}

} // namespace xmsg
} // namespace mixr
