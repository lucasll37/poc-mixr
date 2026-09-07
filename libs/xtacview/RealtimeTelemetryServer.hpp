#ifndef __xtacview_RealtimeTelemetryServer_H__
#define __xtacview_RealtimeTelemetryServer_H__

#include <cstddef>
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
//------------------------------------------------------------------------------
// Propriedades ACMI declaradas na PRIMEIRA aparicao de um objeto.
//
// SEMANTICA DO FORMATO (documentacao oficial do Tacview) -- errar isto e o
// que faz uma aeronave aparecer como "objeto desconhecido" no replay:
//
//   Name     = o MODELO do objeto, na notacao ICAO/OTAN ("F-4E", "F-16C",
//              "C172"). E por este campo (junto com Type) que o Tacview
//              procura a aeronave na sua base de dados e escolhe o modelo
//              3D/icone. Um valor que a base nao conhece cai na forma
//              generica do Type -- que ainda e uma aeronave.
//   Type     = a taxonomia ("Air+FixedWing", "Misc+Decoy+Chaff", ...).
//   CallSign = o indicativo da aeronave (o nome do player, aqui).
//   Pilot    = o piloto -- e o que o Tacview mostra no rotulo do objeto.
//
// O ERRO ANTERIOR desta lib era mandar o nome do player em 'Name'
// ("Name=uav1"): o Tacview procurava "uav1" na base, nao achava, e
// desenhava um objeto generico. O nome do player pertence a CallSign/Pilot.
//------------------------------------------------------------------------------
struct ObjectInfo
{
   std::string model;      // Name=  (modelo; vazio => propriedade omitida)
   std::string type;       // Type=
   std::string color;      // Color=
   std::string callsign;   // CallSign= e Pilot= (vazio => omitidas)
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

   //---------------------------------------------------------------------
   // Introspeccao do socket -- so leitura, para a aba "Tempo Nao-Critico"
   // do ./app poder mostrar o estado REAL do transporte em vez de um
   // "ligado/desligado" que nao distingue "ninguem conectou ainda" de
   // "o Tacview caiu no meio". Tudo que devolve aqui e contador ja
   // mantido pelo proprio caminho de escrita: nada e amostrado a mais.
   //---------------------------------------------------------------------
   // O socket de escuta subiu de fato? (start() pode falhar no bind -- porta
   // ocupada por outra poc -- e a gravacao em arquivo continuar valendo; ver
   // TacviewOutput::initIfNeeded().) 'listenHost/listenPort' devolvem o que
   // foi CONFIGURADO, com socket ou sem.
   bool isListening() const                 { return listenFd_ >= 0; }
   int listenPort() const                   { return listenPort_; }
   const std::string& listenHost() const    { return listenHost_; }
   const std::string& callsign() const      { return callsign_; }
   const std::string& recordingPath() const { return recordingPath_; }

   // Quantos clientes ja conectaram desde o start (nao quantos estao
   // conectados -- isso e isConnected()). Distingue "o Tacview nunca veio"
   // de "veio e caiu", que e a duvida real ao depurar a exportacao.
   unsigned long connectionCount() const    { return connectionCount_; }

   unsigned long bytesSent() const          { return bytesSent_; }
   unsigned long linesWritten() const       { return linesWritten_; }
   unsigned long framesEmitted() const      { return framesEmitted_; }

   // Objetos ja declarados (Name/Type/Color emitidos) para cada destino.
   std::size_t objectsOnSocket() const      { return knownObjectsSocket_.size(); }
   std::size_t objectsInFile() const        { return knownObjectsFile_.size(); }

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

   // Feixe/varredura de radar de um objeto ja declarado (RadarAzimuth/
   // RadarElevation sao relativos ao PROPRIO objeto, nao ao norte -- ver
   // TacviewOutput::updateRadarScan()). Angulos em graus, alcance em metros.
   void updateRadarBeam(const std::uint32_t objectId,
                        const double azimuthDeg, const double elevationDeg, const double rangeM,
                        const double hBeamwidthDeg, const double vBeamwidthDeg);

   // Remove um objeto da cena ("-<idHex>"), para que efeitos/armas expirados
   // nao fiquem "fantasmas" parados na ultima posicao conhecida.
   void removeObject(const std::uint32_t objectId);

   void stop();

private:
   void closeClient();
   bool sendRaw(const std::string& data);
   void writeLine(const std::string& lineWithoutNewline);

   std::string callsign_{"poc-mixr"};
   std::string listenHost_;
   std::string recordingPath_;
   int listenPort_{-1};
   int listenFd_{-1};
   int clientFd_{-1};
   unsigned long connectionCount_{};
   unsigned long bytesSent_{};
   unsigned long linesWritten_{};
   unsigned long framesEmitted_{};
   std::unordered_set<std::uint32_t> knownObjectsSocket_;
   std::unordered_set<std::uint32_t> knownObjectsFile_;
   std::ofstream file_;
};

}
}

#endif
