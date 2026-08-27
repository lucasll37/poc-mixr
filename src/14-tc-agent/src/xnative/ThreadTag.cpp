#include "xnative/ThreadTag.hpp"

#include <sched.h>

#include <map>
#include <mutex>
#include <thread>

namespace mixr {
namespace xnative {

namespace {
std::mutex g_tagMutex;
std::map<std::thread::id, int> g_tags;
int g_nextTag{};
}

int threadTag()
{
   // Cache por thread: o mutex global so e tocado UMA vez por thread, na
   // primeira chamada. Sem isto haveria um lock global no caminho quente
   // (todo player, todo frame) -- exatamente o tipo de serializacao que
   // anularia o pool de threads do framework.
   static thread_local int cachedTag{-1};
   if (cachedTag >= 0) return cachedTag;

   const std::thread::id id{std::this_thread::get_id()};

   std::lock_guard<std::mutex> lock(g_tagMutex);
   const auto it = g_tags.find(id);
   if (it != g_tags.end()) {
      cachedTag = it->second;
   } else {
      cachedTag = g_nextTag++;
      g_tags[id] = cachedTag;
   }
   return cachedTag;
}

int currentCpu()
{
   return ::sched_getcpu();
}

} // namespace xnative
} // namespace mixr
