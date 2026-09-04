#include "xtacview/RealtimeTelemetryServer.hpp"

#include "xlog/Log.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace mixr {
namespace xtacview {

namespace {

// Handshake do protocolo Real-Time Telemetry.
//
// A documentacao oficial (tacview.net/documentation/realtime) descreve como
// se a ultima linha nao levasse '\n' antes do '\0'; isso NAO conecta no
// Tacview real ("real-time telemetry not compatible with the host exporter").
// O formato abaixo -- todas as linhas terminando em '\n', com o '\0' como um
// byte extra e separado depois do ultimo '\n' -- foi confirmado contra uma
// implementacao de referencia que de fato conecta
// (github.com/xutter/tacview-toolset/.../dataserver.py).
std::string buildHandshake(const std::string& hostUsername)
{
   std::string h;
   h += "XtraLib.Stream.0\n";
   h += "Tacview.RealTimeTelemetry.0\n";
   h += hostUsername;
   h += '\n';
   h += '\0';
   return h;
}

std::string isoUtcNow()
{
   const std::time_t t{std::time(nullptr)};
   std::tm tm{};
   gmtime_r(&t, &tm);
   std::ostringstream oss;
   oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
   return oss.str();
}

std::string acmiHeader()
{
   std::ostringstream header;
   header << "FileType=text/acmi/tacview\n"
          << "FileVersion=2.2\n"
          << "0,ReferenceTime=" << isoUtcNow() << "\n";
   return header.str();
}

}

RealtimeTelemetryServer::~RealtimeTelemetryServer()
{
   stop();
}

bool RealtimeTelemetryServer::start(const std::string& host, const int port, const std::string& callsign)
{
   listenHost_ = host;
   listenPort_ = port;
   callsign_ = callsign;

   listenFd_ = ::socket(AF_INET, SOCK_STREAM, 0);
   if (listenFd_ < 0) {
      LOG(ERROR) << "[tacview] socket() failed: " << std::strerror(errno);
      return false;
   }

   const int yes{1};
   ::setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

   sockaddr_in addr{};
   addr.sin_family = AF_INET;
   addr.sin_port = htons(static_cast<std::uint16_t>(port));
   if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
      LOG(ERROR) << "[tacview] invalid host: " << host;
      return false;
   }

   if (::bind(listenFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
      LOG(ERROR) << "[tacview] bind() failed on " << host << ":" << port
                 << ": " << std::strerror(errno);
      return false;
   }

   if (::listen(listenFd_, 1) < 0) {
      LOG(ERROR) << "[tacview] listen() failed: " << std::strerror(errno);
      return false;
   }

   // A simulacao nao pode travar esperando o Tacview conectar.
   ::fcntl(listenFd_, F_SETFL, O_NONBLOCK);

   LOG(INFO) << "[tacview] Real-Time Telemetry escutando em " << host << ":" << port
             << " (Tacview: File > Real-Time Telemetry)";
   LOG(INFO) << "[tacview] se o binario roda no WSL2 e o Tacview no Windows e 127.0.0.1 "
             << "nao conectar, use `hostname -I` aqui dentro do WSL2 e conecte em "
             << "<esse-ip>:" << port;
   return true;
}

bool RealtimeTelemetryServer::startRecording(const std::string& filePath)
{
   recordingPath_ = filePath;
   file_.open(filePath, std::ios::out | std::ios::trunc);
   if (!file_.is_open()) {
      LOG(ERROR) << "[tacview] falha ao abrir " << filePath << " para gravacao";
      return false;
   }
   file_ << acmiHeader();
   file_.flush();
   LOG(INFO) << "[tacview] gravando missao em " << filePath;
   return true;
}

void RealtimeTelemetryServer::acceptIfNeeded()
{
   if (isConnected() || listenFd_ < 0) return;

   const int fd{::accept(listenFd_, nullptr, nullptr)};
   if (fd < 0) return;   // ninguem tentando conectar agora

   clientFd_ = fd;
   knownObjectsSocket_.clear();

   // Os DOIS timeouts vao aqui, ANTES do primeiro sendRaw() -- o de escrita
   // inclusive, e ele nao e decorativo.
   //
   // ARMADILHA MEDIDA (nao redescobrir): sem SO_SNDTIMEO, um cliente que
   // CONECTA e para de ler (Tacview minimizado, maquina do cliente
   // engasgada, link caido sem FIN) enche o buffer do socket e o ::send() de
   // sendRaw() bloqueia PARA SEMPRE. Esse send roda dentro de
   // OutputHandler::processQueue(), alcancado por station->updateData() --
   // ou seja, dentro do laco de background da aplicacao. Reproduzido no
   // ./app com um cliente de teste que so conecta: a thread do laco travou
   // no send, a main travou no join() dela, o terminal nunca voltou, e a
   // thread de tempo critico seguiu enfileirando registros numa fila que e
   // uma base::List SEM TETO (RSS subindo ~1,9 MB/s). Um teto de escrita
   // transforma isso em "cliente morto, fecha e segue".
   timeval sendTimeout{1, 0};
   ::setsockopt(clientFd_, SOL_SOCKET, SO_SNDTIMEO, &sendTimeout, sizeof(sendTimeout));

   // Idem para a leitura: nunca travar o laco caso o cliente nao mande nada.
   timeval recvTimeout{1, 0};
   ::setsockopt(clientFd_, SOL_SOCKET, SO_RCVTIMEO, &recvTimeout, sizeof(recvTimeout));

   if (!sendRaw(buildHandshake(callsign_))) {
      closeClient();
      return;
   }

   // Consome o handshake de volta do Tacview.
   char discard[256];
   ::recv(clientFd_, discard, sizeof(discard), 0);

   if (!sendRaw(acmiHeader())) {
      closeClient();
      return;
   }

   connectionCount_ += 1;
   LOG(INFO) << "[tacview] cliente conectado, transmitindo telemetria";
}

void RealtimeTelemetryServer::beginFrame(const double simTimeSec)
{
   framesEmitted_ += 1;
   std::ostringstream line;
   line << "#" << std::fixed << std::setprecision(2) << simTimeSec;
   writeLine(line.str());
   if (file_.is_open()) file_.flush();   // sobrevive a um crash sem perder o replay
}

void RealtimeTelemetryServer::updateObject(const std::uint32_t objectId,
                                           const double lonDeg, const double latDeg, const double altM,
                                           const double rollDeg, const double pitchDeg, const double yawDeg,
                                           const ObjectInfo* const info)
{
   std::ostringstream core;
   core << std::hex << objectId << std::dec
        << ",T=" << std::fixed << std::setprecision(7)
        << lonDeg << "|" << latDeg << "|" << std::setprecision(1) << altM
        << "|" << std::setprecision(2) << rollDeg << "|" << pitchDeg << "|" << yawDeg;

   std::string propsSuffix;
   if (info != nullptr) {
      std::ostringstream p;
      // 'Name' so entra se houver um modelo de verdade: mandar um nome que
      // a base do Tacview nao conhece e pior do que omitir (omitindo, ele
      // usa a forma generica do Type -- ver ObjectInfo no .hpp).
      if (!info->model.empty()) p << ",Name=" << info->model;
      p << ",Type=" << info->type << ",Color=" << info->color;
      if (!info->callsign.empty()) {
         p << ",CallSign=" << info->callsign << ",Pilot=" << info->callsign;
      }
      propsSuffix = p.str();
   }

   if (file_.is_open()) {
      const bool firstForFile{knownObjectsFile_.insert(objectId).second};
      file_ << core.str() << (firstForFile ? propsSuffix : "") << "\n";
   }

   if (isConnected()) {
      const bool firstForSocket{knownObjectsSocket_.insert(objectId).second};
      if (!sendRaw(core.str() + (firstForSocket ? propsSuffix : "") + "\n")) closeClient();
   }
}

void RealtimeTelemetryServer::updateRadarBeam(const std::uint32_t objectId,
                                              const double azimuthDeg, const double elevationDeg,
                                              const double rangeM,
                                              const double hBeamwidthDeg, const double vBeamwidthDeg)
{
   std::ostringstream line;
   line << std::hex << objectId << std::dec
        << ",RadarMode=1"
        << std::fixed << std::setprecision(2)
        << ",RadarAzimuth=" << azimuthDeg
        << ",RadarElevation=" << elevationDeg
        << std::setprecision(1)
        << ",RadarRange=" << rangeM
        << std::setprecision(2)
        << ",RadarHorizontalBeamwidth=" << hBeamwidthDeg
        << ",RadarVerticalBeamwidth=" << vBeamwidthDeg;
   writeLine(line.str());
}

void RealtimeTelemetryServer::logEvent(const std::uint32_t objectId, const std::string& text)
{
   std::ostringstream line;
   line << "0,Event=Message|" << std::hex << objectId << std::dec << "|" << text;
   writeLine(line.str());
}

void RealtimeTelemetryServer::removeObject(const std::uint32_t objectId)
{
   std::ostringstream line;
   line << "-" << std::hex << objectId << std::dec;
   writeLine(line.str());

   // Se o mesmo id reaparecer depois, deve ser redeclarado (Name/Type/Color)
   // como se fosse a primeira aparicao.
   knownObjectsFile_.erase(objectId);
   knownObjectsSocket_.erase(objectId);
}

void RealtimeTelemetryServer::stop()
{
   closeClient();
   if (listenFd_ >= 0) {
      ::close(listenFd_);
      listenFd_ = -1;
   }
   if (file_.is_open()) {
      file_.flush();
      file_.close();
   }
}

void RealtimeTelemetryServer::closeClient()
{
   if (clientFd_ >= 0) {
      ::close(clientFd_);
      clientFd_ = -1;
      LOG(INFO) << "[tacview] cliente desconectado";
   }
}

bool RealtimeTelemetryServer::sendRaw(const std::string& data)
{
   if (clientFd_ < 0) return false;

   std::size_t sent{0};
   while (sent < data.size()) {
      const ssize_t n{::send(clientFd_, data.data() + sent, data.size() - sent, MSG_NOSIGNAL)};
      if (n < 0 && errno == EINTR) continue;   // sinal no meio, nao e erro
      // Com o SO_SNDTIMEO de acceptIfNeeded(), um cliente que parou de ler
      // devolve EAGAIN/EWOULDBLOCK depois do teto. Tratamos como cliente
      // morto (mesmo caminho de n == 0 / erro): quem chama -- writeLine() --
      // ja faz closeClient(), e a simulacao segue sem exportador.
      if (n <= 0) return false;
      sent += static_cast<std::size_t>(n);
      bytesSent_ += static_cast<unsigned long>(n);
   }
   return true;
}

void RealtimeTelemetryServer::writeLine(const std::string& lineWithoutNewline)
{
   linesWritten_ += 1;
   if (file_.is_open()) {
      file_ << lineWithoutNewline << "\n";
   }
   if (isConnected()) {
      if (!sendRaw(lineWithoutNewline + "\n")) closeClient();
   }
}

}
}
