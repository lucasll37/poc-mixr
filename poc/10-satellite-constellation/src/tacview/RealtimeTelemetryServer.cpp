#include "tacview/RealtimeTelemetryServer.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace tacview {

namespace {

// Handshake do protocolo Real-Time Telemetry -- ver nota detalhada na
// memoria do projeto / CLAUDE.md (poc/04): a doc oficial sugere que a
// ultima linha nao leva '\n' antes do '\0', mas isso NAO conecta no
// Tacview de verdade; a ultima linha tambem termina em '\n', e o '\0' e um
// byte extra e separado (confirmado contra github.com/xutter/tacview-toolset).
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

} // namespace

RealtimeTelemetryServer::RealtimeTelemetryServer(const std::string& host, const int port)
   : host_(host), port_(port)
{
}

RealtimeTelemetryServer::~RealtimeTelemetryServer()
{
   stop();
}

bool RealtimeTelemetryServer::start()
{
   listenFd_ = ::socket(AF_INET, SOCK_STREAM, 0);
   if (listenFd_ < 0) {
      std::cerr << "[tacview] socket() failed: " << std::strerror(errno) << std::endl;
      return false;
   }

   const int yes{1};
   ::setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

   sockaddr_in addr{};
   addr.sin_family = AF_INET;
   addr.sin_port = htons(static_cast<std::uint16_t>(port_));
   if (::inet_pton(AF_INET, host_.c_str(), &addr.sin_addr) != 1) {
      std::cerr << "[tacview] invalid host: " << host_ << std::endl;
      return false;
   }

   if (::bind(listenFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
      std::cerr << "[tacview] bind() failed on " << host_ << ":" << port_
                << ": " << std::strerror(errno) << std::endl;
      return false;
   }

   if (::listen(listenFd_, 1) < 0) {
      std::cerr << "[tacview] listen() failed: " << std::strerror(errno) << std::endl;
      return false;
   }

   // Accept nao-bloqueante: a simulacao nao pode travar esperando o Tacview.
   ::fcntl(listenFd_, F_SETFL, O_NONBLOCK);

   std::cout << "[tacview] Real-Time Telemetry escutando em " << host_ << ":" << port_
             << " (conecte o Tacview em File > Real-Time Telemetry)" << std::endl;
   return true;
}

bool RealtimeTelemetryServer::startRecording(const std::string& filePath)
{
   file_.open(filePath, std::ios::out | std::ios::trunc);
   if (!file_.is_open()) {
      std::cerr << "[tacview] falha ao abrir " << filePath << " para gravacao" << std::endl;
      return false;
   }
   file_ << acmiHeader();
   file_.flush();
   std::cout << "[tacview] gravando missao em " << filePath << std::endl;
   return true;
}

void RealtimeTelemetryServer::acceptIfNeeded()
{
   if (isConnected() || listenFd_ < 0) return;

   const int fd{::accept(listenFd_, nullptr, nullptr)};
   if (fd < 0) return; // ninguem tentando conectar agora, tudo bem

   clientFd_ = fd;
   knownObjectsSocket_.clear();

   if (!sendRaw(buildHandshake("poc-mixr/satellite-constellation"))) {
      closeClient();
      return;
   }

   // Consome o handshake que o Tacview manda de volta, com timeout curto
   // pra nunca travar o loop da simulacao caso o cliente nao mande nada.
   timeval recvTimeout{1, 0}; // 1s
   ::setsockopt(clientFd_, SOL_SOCKET, SO_RCVTIMEO, &recvTimeout, sizeof(recvTimeout));
   char discard[256];
   ::recv(clientFd_, discard, sizeof(discard), 0);

   if (!sendRaw(acmiHeader())) {
      closeClient();
      return;
   }

   std::cout << "[tacview] cliente conectado, transmitindo telemetria" << std::endl;
}

void RealtimeTelemetryServer::beginFrame(const double simTimeSec)
{
   std::ostringstream line;
   line << "#" << std::fixed << std::setprecision(2) << simTimeSec;
   writeLine(line.str());
   if (file_.is_open()) file_.flush(); // sobrevive a um crash sem perder o replay
}

void RealtimeTelemetryServer::updateObject(const std::uint32_t objectId,
                                            const double lonDeg, const double latDeg, const double altM,
                                            const double rollDeg, const double pitchDeg, const double yawDeg,
                                            const ObjectInfo* info)
{
   std::ostringstream core;
   core << std::hex << objectId << std::dec
        << ",T=" << std::fixed << std::setprecision(7)
        << lonDeg << "|" << latDeg << "|" << std::setprecision(1) << altM
        << "|" << std::setprecision(2) << rollDeg << "|" << pitchDeg << "|" << yawDeg;

   std::string propsSuffix;
   if (info != nullptr) {
      std::ostringstream p;
      p << ",Name=" << info->name << ",Type=" << info->type << ",Color=" << info->color;
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

   // se o mesmo id for reutilizado depois, deve ser redeclarado (Name/
   // Type/Color) como se fosse a primeira aparicao de novo.
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
      std::cout << "[tacview] cliente desconectado" << std::endl;
   }
}

bool RealtimeTelemetryServer::sendRaw(const std::string& data)
{
   if (clientFd_ < 0) return false;

   std::size_t sent{0};
   while (sent < data.size()) {
      const ssize_t n{::send(clientFd_, data.data() + sent, data.size() - sent, MSG_NOSIGNAL)};
      if (n <= 0) return false;
      sent += static_cast<std::size_t>(n);
   }
   return true;
}

void RealtimeTelemetryServer::writeLine(const std::string& lineWithoutNewline)
{
   if (file_.is_open()) {
      file_ << lineWithoutNewline << "\n";
   }
   if (isConnected()) {
      if (!sendRaw(lineWithoutNewline + "\n")) closeClient();
   }
}

} // namespace tacview
