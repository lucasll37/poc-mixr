#pragma once

#include <cstdint>
#include <string>
#include <unordered_set>

namespace tacview {

// Propriedades opcionais de um objeto, enviadas apenas na primeira vez que
// o objeto aparece no stream (nao mudam quadro a quadro).
struct ObjectInfo
{
   std::string name;
   std::string type;
   std::string color;
};

//------------------------------------------------------------------------------
// Class: RealtimeTelemetryServer
//
// Implementa o lado "host" do protocolo publico Tacview Real-Time Telemetry
// (https://www.tacview.net/documentation/realtime/en/): um servidor TCP que
// aceita UMA conexao do Tacview (que atua como cliente), faz o handshake
// (XtraLib.Stream.0 / Tacview.RealTimeTelemetry.0) e em seguida transmite um
// stream ACMI 2.2 (texto) com as posicoes dos objetos, quadro a quadro.
//
// Uso tipico, uma vez por tick de simulacao:
//
//   server.acceptIfNeeded();
//   if (server.isConnected()) {
//      server.beginFrame(simTime);
//      server.updateObject(0x101, lon, lat, altM, roll, pitch, yaw, info);
//   }
//
// Nao bloqueante: se o Tacview ainda nao conectou (ou desconectou), as
// chamadas acima simplesmente viram no-ops -- a simulacao roda normalmente
// com ou sem um cliente de telemetria conectado.
//------------------------------------------------------------------------------
class RealtimeTelemetryServer
{
public:
   RealtimeTelemetryServer(const std::string& host, const int port);
   ~RealtimeTelemetryServer();

   // Cria, faz bind e coloca o socket de escuta em modo nao-bloqueante.
   // Retorna false em caso de erro (porta em uso, etc.).
   bool start();

   // Tenta aceitar uma conexao pendente (nao bloqueia se nao houver
   // nenhuma). Se aceitar, faz o handshake e envia o cabecalho ACMI.
   void acceptIfNeeded();

   bool isConnected() const { return clientFd_ >= 0; }

   // Inicia um novo quadro de tempo (linha "#<segundos>").
   void beginFrame(const double simTimeSec);

   // Atualiza (ou declara, na primeira chamada) um objeto neste quadro.
   // Angulos em graus, altitude em metros, lon/lat em graus decimais.
   void updateObject(const std::uint32_t objectId,
                      const double lonDeg, const double latDeg, const double altM,
                      const double rollDeg, const double pitchDeg, const double yawDeg,
                      const ObjectInfo* info = nullptr);

   void stop();

private:
   void closeClient();
   bool sendRaw(const std::string& data);

   std::string host_;
   int port_{};
   int listenFd_{-1};
   int clientFd_{-1};
   std::unordered_set<std::uint32_t> knownObjects_;
};

} // namespace tacview
