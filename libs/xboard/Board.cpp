#include "xboard/Board.hpp"

#include <sched.h>

#include <map>
#include <mutex>
#include <thread>

namespace mixr {
namespace xboard {

namespace {

// Um mutex so para tudo: escrita e leitura sao ambas raras (uma por ciclo de
// decisao, uma por dump) e o mapa e minusculo -- 4 entradas.
std::mutex g_mutex;
std::map<int, Readout> g_board;

std::mutex g_tagMutex;
std::map<std::thread::id, int> g_tags;
int g_nextTag{};

} // namespace

void setBehaviorLabel(const int playerId, const std::string& label)
{
   std::lock_guard<std::mutex> lock(g_mutex);
   g_board[playerId].label = label;
}

void bumpDecisionCount(const int playerId)
{
   std::lock_guard<std::mutex> lock(g_mutex);
   g_board[playerId].decisions += 1;
}

void setThreadTag(const int playerId, const int tag)
{
   std::lock_guard<std::mutex> lock(g_mutex);
   g_board[playerId].threadTag = tag;
}

void setAlert(const int playerId, const bool valid,
              const std::string& sender, const std::string& contact)
{
   std::lock_guard<std::mutex> lock(g_mutex);
   Readout& r{g_board[playerId]};
   r.alertValid = valid;
   r.alertSender = sender;
   r.alertContact = contact;
}

void setDatalinkCounters(const int playerId, const long sent, const long received)
{
   std::lock_guard<std::mutex> lock(g_mutex);
   Readout& r{g_board[playerId]};
   r.sent = sent;
   r.received = received;
}

void setRadarScan(const int playerId, const bool valid, const double azDeg, const double elDeg,
                  const double rangeM, const double hBeamDeg, const double vBeamDeg)
{
   std::lock_guard<std::mutex> lock(g_mutex);
   Readout& r{g_board[playerId]};
   r.radarValid = valid;
   r.radarAzDeg = azDeg;
   r.radarElDeg = elDeg;
   r.radarRangeM = rangeM;
   r.radarHBeamDeg = hBeamDeg;
   r.radarVBeamDeg = vBeamDeg;
}

Readout get(const int playerId)
{
   std::lock_guard<std::mutex> lock(g_mutex);
   const auto it = g_board.find(playerId);
   return (it != g_board.end()) ? it->second : Readout{};
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

} // namespace xboard
} // namespace mixr
