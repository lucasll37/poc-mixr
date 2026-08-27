#include "xnative/BehaviorBoard.hpp"

#include <map>
#include <mutex>

namespace mixr {
namespace xnative {

namespace {
std::mutex g_labelMutex;
std::map<int, std::string> g_labels;
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

} // namespace xnative
} // namespace mixr
