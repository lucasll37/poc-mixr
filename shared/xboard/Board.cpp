#include "xboard/Board.hpp"

#include <map>
#include <mutex>

namespace mixr {
namespace xboard {

namespace {

// Um mutex so para tudo: escrita e leitura sao ambas raras (uma por ciclo de
// decisao, uma por dump) e o mapa e minusculo -- 4 entradas.
std::mutex g_mutex;
std::map<int, Readout> g_board;

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

Readout get(const int playerId)
{
   std::lock_guard<std::mutex> lock(g_mutex);
   const auto it = g_board.find(playerId);
   return (it != g_board.end()) ? it->second : Readout{};
}

} // namespace xboard
} // namespace mixr
