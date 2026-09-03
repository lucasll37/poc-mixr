#include "xrlbridge/RLBridge.hpp"

#include <mutex>

namespace mixr {
namespace xrlbridge {

namespace {

// Um mutex so para os dois campos -- sem mapa por player id (ver o "porque"
// no cabecalho de RLBridge.hpp: v1 e um unico agente RL por processo).
std::mutex g_mutex;
Command g_command;
Observation g_observation;

} // namespace

void setPendingCommand(const Command& cmd)
{
   std::lock_guard<std::mutex> lock(g_mutex);
   g_command = cmd;
}

Command getPendingCommand()
{
   std::lock_guard<std::mutex> lock(g_mutex);
   return g_command;
}

void setObservation(const Observation& obs)
{
   std::lock_guard<std::mutex> lock(g_mutex);
   g_observation = obs;
}

Observation getObservation()
{
   std::lock_guard<std::mutex> lock(g_mutex);
   return g_observation;
}

} // namespace xrlbridge
} // namespace mixr
