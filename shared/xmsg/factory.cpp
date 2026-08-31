#include "xmsg/factory.hpp"

#include "mixr/base/Object.hpp"

#include "xmsg/MsgChanged.hpp"
#include "xmsg/MsgFeed.hpp"
#include "xmsg/MsgFileSink.hpp"
#include "xmsg/MsgRate.hpp"
#include "xmsg/MsgReport.hpp"
#include "xmsg/MsgThreshold.hpp"

namespace mixr {
namespace xmsg {

// Condition e MsgSink sao abstratas -- nao entram aqui de proposito: nao ha o
// que o parser EDL construa a partir delas.
base::Object* factory(const std::string& name)
{
   base::Object* obj{};

   if (name == MsgFeed::getFactoryName())          obj = new MsgFeed();
   else if (name == MsgReport::getFactoryName())   obj = new MsgReport();
   else if (name == MsgThreshold::getFactoryName()) obj = new MsgThreshold();
   else if (name == MsgChanged::getFactoryName())  obj = new MsgChanged();
   else if (name == MsgRate::getFactoryName())     obj = new MsgRate();
   else if (name == MsgFileSink::getFactoryName()) obj = new MsgFileSink();

   return obj;
}

} // namespace xmsg
} // namespace mixr
