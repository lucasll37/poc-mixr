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


//------------------------------------------------------------------------------
// O contrato de dados (ver xrlbridge/ObservationFields.hpp).
//
// As tres funcoes abaixo sao EXPANSOES da mesma macro. Nenhuma lista de campos
// e escrita a mao aqui -- e esse o ponto.
//------------------------------------------------------------------------------

namespace {

// A contagem tem de bater com a macro. Um campo acrescentado sem atualizar
// XRLBRIDGE_OBSERVATION_SIZE para aqui, em tempo de compilacao.
constexpr int contarCampos()
{
   int n{};
#define XRLBRIDGE_F(nome) ++n;
#define XRLBRIDGE_B(nome) ++n;
   XRLBRIDGE_OBSERVATION_FIELDS
#undef XRLBRIDGE_F
#undef XRLBRIDGE_B
   return n;
}
static_assert(contarCampos() == XRLBRIDGE_OBSERVATION_SIZE,
              "XRLBRIDGE_OBSERVATION_SIZE nao bate com a lista de campos");

} // namespace

std::vector<std::string> observationFieldNames()
{
   std::vector<std::string> nomes;
   nomes.reserve(XRLBRIDGE_OBSERVATION_SIZE);
#define XRLBRIDGE_F(nome) nomes.emplace_back(#nome);
#define XRLBRIDGE_B(nome) nomes.emplace_back(#nome);
   XRLBRIDGE_OBSERVATION_FIELDS
#undef XRLBRIDGE_F
#undef XRLBRIDGE_B
   return nomes;
}

void packObservation(const Observation& obs, float* const out)
{
   if (out == nullptr) return;
   int i{};
#define XRLBRIDGE_F(nome) out[i++] = static_cast<float>(obs.nome);
#define XRLBRIDGE_B(nome) out[i++] = obs.nome ? 1.0F : 0.0F;
   XRLBRIDGE_OBSERVATION_FIELDS
#undef XRLBRIDGE_F
#undef XRLBRIDGE_B
}

Command unscaleCommand(const float* const normalized3)
{
   Command cmd;
   if (normalized3 == nullptr) return cmd;

   int i{};
   // t = (n+1)/2 leva [-1,1] em [0,1]; dai escala para [low,high]. O recorte
   // vem ANTES: uma saida fora de escala nao pode virar um comando absurdo.
#define XRLBRIDGE_A(nome, low, high)                                    \
   {                                                                    \
      float n{normalized3[i++]};                                        \
      if (n < -1.0F) n = -1.0F;                                          \
      if (n >  1.0F) n =  1.0F;                                          \
      const double t{(static_cast<double>(n) + 1.0) * 0.5};             \
      cmd.nome = (low) + t * ((high) - (low));                          \
   }
   XRLBRIDGE_ACTION_FIELDS
#undef XRLBRIDGE_A
   return cmd;
}

} // namespace xrlbridge
} // namespace mixr
