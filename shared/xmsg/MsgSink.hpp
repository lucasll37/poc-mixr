#ifndef __xmsg_MsgSink_H__
#define __xmsg_MsgSink_H__

#include "mixr/base/Object.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace mixr {
namespace base { class PairStream; }

namespace xmsg {

//------------------------------------------------------------------------------
// Class: MsgSink
//
// Description: Um destino de mensagens. Base abstrata.
//
// Slots:
//    messages   <PairStream>   ! nomes das mensagens aceitas; vazio = todas
//
// O FILTRO POR ASSINANTE e a propriedade que vale copiar do OutputHandler
// nativo do MIXR: o mesmo feed alimenta destinos diferentes com recortes
// diferentes, sem duplicar a amostragem. Um sink de arquivo pode gravar tudo
// enquanto outro, no futuro, manda so os alarmes para a rede.
//
// Acrescentar um destino novo (console, UDP sobre base::network) e uma classe
// e uma linha na factory -- nada mais no subsistema muda.
//------------------------------------------------------------------------------
class MsgSink : public base::Object
{
   DECLARE_SUBCLASS(MsgSink, base::Object)

public:
   MsgSink();

   bool accepts(const std::string& msgName) const;

   virtual bool open() = 0;
   virtual void write(const char* line, std::size_t len) = 0;
   virtual void tick(double dt) = 0;      // flush periodico, se o destino tiver
   virtual void close() = 0;

   long failed() const  { return failed_; }

protected:
   void countFailure()  { ++failed_; }

private:
   bool setSlotMessages(const base::PairStream* const);

   std::vector<std::string> accept_;
   long failed_{};
};

} // namespace xmsg
} // namespace mixr

#endif
