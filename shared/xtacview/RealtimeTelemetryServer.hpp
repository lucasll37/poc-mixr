#ifndef __xtacview_RealtimeTelemetryServer_H__
#define __xtacview_RealtimeTelemetryServer_H__

#include <cstdint>
#include <fstream>
#include <string>
#include <unordered_set>

namespace mixr {
namespace xtacview {

//------------------------------------------------------------------------------
// Struct: ObjectInfo
// Description: Propriedades declaradas apenas na primeira aparicao de um
//              objeto no stream ACMI (nao mudam quadro a quadro).
//------------------------------------------------------------------------------
struct ObjectInfo
{
   std::string name;
   std::string type;
   std::string color;
};

//------------------------------------------------------------------------------
// Class: RealtimeTelemetryServer
// Description: Transporte do protocolo publico "Tacview Real-Time Telemetry"
//              (handshake XtraLib.Stream.0 + stream ACMI 2.2), com gravacao
//              simultanea em um arquivo .acmi local.
//
// Esta e a UNICA copia deste servidor no repositorio -- antes existia uma por
// poc (04/05/07/08/09/10), com quatro variantes divergentes.
//
// NAO e um mixr::base::NetHandler de proposito: o Tacview e o consumidor
// final e fala um protocolo proprio (handshake + texto ACMI), entao nao ha
// nada a ganhar em pluga-lo num RecorderNetOutput, que serializa protobuf.
// O ganho de padrao esta em quem ALIMENTA este servidor -- ver TacviewOutput,
// que e um mixr::recorder::OutputHandler de verdade.
//
// O accept() e nao-bloqueante: a simulacao roda com ou sem o Tacview
// conectado, e reconecta sozinha se o cliente cair.
//------------------------------------------------------------------------------
class RealtimeTelemetryServer
{
public:
   RealtimeTelemetryServer() = default;
   ~RealtimeTelemetryServer();

   RealtimeTelemetryServer(const RealtimeTelemetryServer&) = delete;
   RealtimeTelemetryServer& operator=(const RealtimeTelemetryServer&) = delete;

   // Cria, faz bind e coloca o socket de escuta em modo nao-bloqueante.
   // 'host' deve ser 0.0.0.0 quando o Tacview roda no Windows e o binario
   // no WSL2 (ver nota de rede no CLAUDE.md).
   bool start(const std::string& host, const int port, const std::string& callsign);

   // Abre o arquivo .acmi local e escreve o cabecalho imediatamente, de modo
   // que o replay fique completo mesmo que ninguem conecte o Tacview.
   bool startRecording(const std::string& filePath);

   // Aceita uma conexao pendente, se houver (nunca bloqueia).
   void acceptIfNeeded();

   bool isConnected() const                 { return clientFd_ >= 0; }
   bool isRecording() const                 { return file_.is_open(); }
   bool isActive() const                    { return isConnected() || isRecording(); }

   // Novo quadro de tempo (linha "#<segundos>").
   void beginFrame(const double simTimeSec);

   // Atualiza (ou declara, na primeira vez que cada destino ve este id) um
   // objeto. Angulos em graus, altitude em metros, lon/lat em graus decimais.
   void updateObject(const std::uint32_t objectId,
                     const double lonDeg, const double latDeg, const double altM,
                     const double rollDeg, const double pitchDeg, const double yawDeg,
                     const ObjectInfo* const info = nullptr);

   // Evento textual associado a um objeto ("0,Event=Message|<idHex>|<texto>").
   void logEvent(const std::uint32_t objectId, const std::string& text);

   // Remove um objeto da cena ("-<idHex>"), para que efeitos/armas expirados
   // nao fiquem "fantasmas" parados na ultima posicao conhecida.
   void removeObject(const std::uint32_t objectId);

   void stop();

private:
   void closeClient();
   bool sendRaw(const std::string& data);
   void writeLine(const std::string& lineWithoutNewline);

   std::string callsign_{"poc-mixr"};
   int listenFd_{-1};
   int clientFd_{-1};
   std::unordered_set<std::uint32_t> knownObjectsSocket_;
   std::unordered_set<std::uint32_t> knownObjectsFile_;
   std::ofstream file_;
};

}
}

#endif
