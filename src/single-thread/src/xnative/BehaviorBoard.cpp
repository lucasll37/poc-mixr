#include "xnative/BehaviorBoard.hpp"

#include <map>
#include <mutex>

namespace mixr {
namespace xnative {

namespace {
std::mutex g_labelMutex;
std::map<int, std::string> g_labels;
std::map<int, long> g_decisions;   // mesmo mutex: escrita rara, leitura rara
}

void setBehaviorLabel(const int playerId, const std::string& label)
{
   std::lock_guard<std::mutex> lock(g_labelMutex);
   g_labels[playerId] = label;
}

std::string getBehaviorLabel(const int playerId)
{
   std::lock_guard<std::mutex> lock(g_labelMutex);
   const auto it = g_labels.find(playerId);
   return (it != g_labels.end()) ? it->second : std::string("--");
}

void bumpDecisionCount(const int playerId)
{
   std::lock_guard<std::mutex> lock(g_labelMutex);
   g_decisions[playerId] += 1;
}

long getDecisionCount(const int playerId)
{
   std::lock_guard<std::mutex> lock(g_labelMutex);
   const auto it = g_decisions.find(playerId);
   return (it != g_decisions.end()) ? it->second : 0L;
}

} // namespace xnative
} // namespace mixr
