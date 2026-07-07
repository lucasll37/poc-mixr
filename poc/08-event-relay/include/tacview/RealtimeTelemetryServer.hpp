#pragma once

#include <cstdint>
#include <fstream>
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
// Extensao da versao usada em poc/04-jsbsim-6dof: alem do socket Real-Time
// Telemetry (protocolo publico do Tacview -- handshake XtraLib.Stream.0 /
// Tacview.RealTimeTelemetry.0 + stream ACMI 2.2), agora tambem grava o
// mesmo stream em um arquivo .acmi local, independente de o Tacview estar
// conectado ao vivo -- e o que garante "replay completo da missao" mesmo
// que ninguem tenha aberto o Tacview durante a execucao. Suporta multiplos
// objetos (uma aeronave por updateObject()) e eventos textuais
// (Event=Message, formato ACMI oficial) para correlacionar mudancas de
// formacao/RTB no replay.
//
// Uso tipico, uma vez por tick de simulacao:
//
//   server.acceptIfNeeded();
//   server.beginFrame(simTime);                 // grava no arquivo sempre;
//   for each aircraft:                          // no socket, so se conectado
//      server.updateObject(id, lon, lat, altM, roll, pitch, yaw, info);
//   if (formationChanged) server.logEvent(leadId, "Formation changed to X");
//------------------------------------------------------------------------------
class RealtimeTelemetryServer
{
public:
   RealtimeTelemetryServer(const std::string& host, const int port);
   ~RealtimeTelemetryServer();

   // Cria, faz bind e coloca o socket de escuta em modo nao-bloqueante.
   // Retorna false em caso de erro (porta em uso, etc.).
   bool start();

   // Abre o arquivo .acmi local e escreve o cabecalho ACMI imediatamente
   // (o arquivo fica valido/completo mesmo que o Tacview nunca conecte).
   bool startRecording(const std::string& filePath);

   // Tenta aceitar uma conexao pendente (nao bloqueia se nao houver
   // nenhuma). Se aceitar, faz o handshake e envia o cabecalho ACMI.
   void acceptIfNeeded();

   bool isConnected() const { return clientFd_ >= 0; }

   // Inicia um novo quadro de tempo (linha "#<segundos>"): sempre grava no
   // arquivo (se aberto); manda pro socket so se houver cliente conectado.
   void beginFrame(const double simTimeSec);

   // Atualiza (ou declara, na primeira vez que cada destino -- arquivo ou
   // socket -- ve este objectId) um objeto neste quadro. Angulos em graus,
   // altitude em metros, lon/lat em graus decimais.
   void updateObject(const std::uint32_t objectId,
                      const double lonDeg, const double latDeg, const double altM,
                      const double rollDeg, const double pitchDeg, const double yawDeg,
                      const ObjectInfo* info = nullptr);

   // Evento textual global (ex.: mudanca de formacao, RTB), associado a um
   // objeto -- formato oficial ACMI "0,Event=Message|<idHex>|<texto>".
   void logEvent(const std::uint32_t objectId, const std::string& text);

   void stop();

private:
   void closeClient();
   bool sendRaw(const std::string& data);
   void writeLine(const std::string& lineWithoutNewline);

   std::string host_;
   int port_{};
   int listenFd_{-1};
   int clientFd_{-1};
   std::unordered_set<std::uint32_t> knownObjectsSocket_;
   std::unordered_set<std::uint32_t> knownObjectsFile_;
   std::ofstream file_;
};

} // namespace tacview
