#include "xnative/Log.hpp"

#include <iostream>
#include <mutex>

namespace mixr {
namespace xnative {

namespace {
std::mutex g_logMutex;
bool g_loggingEnabled{true};
}

void setLoggingEnabled(const bool enabled)
{
   std::lock_guard<std::mutex> lock(g_logMutex);
   g_loggingEnabled = enabled;
}

void logLine(const std::string& line)
{
   std::lock_guard<std::mutex> lock(g_logMutex);
   if (!g_loggingEnabled) return;
   std::cout << line << std::endl;
}

} // namespace xnative
} // namespace mixr
