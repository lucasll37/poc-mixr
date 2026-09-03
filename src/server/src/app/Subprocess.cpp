#include "app/Subprocess.hpp"

#include <chrono>
#include <cerrno>
#include <cstddef>

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace app {

namespace {

// Teto de captura por fluxo -- uma simulacao que enlouquecer e imprimir
// megabytes de erro nao pode estourar a memoria do server.
constexpr std::size_t kMaxCapturedBytes{1024 * 1024};

void setNonBlocking(const int fd)
{
   const int flags{fcntl(fd, F_GETFL, 0)};
   fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void drain(const int fd, std::string& sink, bool& eof)
{
   char buf[4096];
   for (;;) {
      const ssize_t n{read(fd, buf, sizeof(buf))};
      if (n > 0) {
         if (sink.size() < kMaxCapturedBytes) sink.append(buf, static_cast<std::size_t>(n));
      } else if (n == 0) {
         eof = true;
         return;
      } else {
         if (errno == EAGAIN || errno == EWOULDBLOCK) return;   // nada agora, tenta depois
         eof = true;                                            // erro de leitura -- trata como fim
         return;
      }
   }
}

} // namespace

SubprocessResult runSubprocess(const std::string& exePath, const std::vector<std::string>& args,
                               const int timeoutSec)
{
   SubprocessResult result;

   int outPipe[2];
   int errPipe[2];
   if (pipe(outPipe) != 0) {
      result.stderrText = "pipe() falhou (stdout)";
      return result;
   }
   if (pipe(errPipe) != 0) {
      close(outPipe[0]); close(outPipe[1]);
      result.stderrText = "pipe() falhou (stderr)";
      return result;
   }

   const pid_t pid{fork()};
   if (pid < 0) {
      close(outPipe[0]); close(outPipe[1]);
      close(errPipe[0]); close(errPipe[1]);
      result.stderrText = "fork() falhou";
      return result;
   }

   if (pid == 0) {
      // --- filho: nunca retorna daqui em diante ---
      dup2(outPipe[1], STDOUT_FILENO);
      dup2(errPipe[1], STDERR_FILENO);
      close(outPipe[0]); close(outPipe[1]);
      close(errPipe[0]); close(errPipe[1]);

      std::vector<char*> argv;
      argv.push_back(const_cast<char*>(exePath.c_str()));
      for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
      argv.push_back(nullptr);

      execv(exePath.c_str(), argv.data());
      _exit(127);   // so chega aqui se execv() falhar (binario ausente, etc.)
   }

   // --- pai ---
   close(outPipe[1]);
   close(errPipe[1]);
   setNonBlocking(outPipe[0]);
   setNonBlocking(errPipe[0]);

   bool outEof{};
   bool errEof{};
   const auto deadline{std::chrono::steady_clock::now() + std::chrono::seconds(timeoutSec)};

   while (!outEof || !errEof) {
      const auto now{std::chrono::steady_clock::now()};
      if (now >= deadline) {
         result.timedOut = true;
         break;
      }
      const int msLeft{static_cast<int>(
         std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count())};

      struct pollfd fds[2];
      int nfds{};
      int outIdx{-1};
      int errIdx{-1};
      if (!outEof) { fds[nfds] = {outPipe[0], POLLIN, 0}; outIdx = nfds; nfds++; }
      if (!errEof) { fds[nfds] = {errPipe[0], POLLIN, 0}; errIdx = nfds; nfds++; }

      const int rc{poll(fds, static_cast<nfds_t>(nfds), msLeft > 0 ? msLeft : 0)};
      if (rc < 0) {
         if (errno == EINTR) continue;
         break;
      }
      if (outIdx >= 0 && (fds[outIdx].revents & (POLLIN | POLLHUP | POLLERR))) {
         drain(outPipe[0], result.stdoutText, outEof);
      }
      if (errIdx >= 0 && (fds[errIdx].revents & (POLLIN | POLLHUP | POLLERR))) {
         drain(errPipe[0], result.stderrText, errEof);
      }
   }

   close(outPipe[0]);
   close(errPipe[0]);

   int status{};
   if (result.timedOut) {
      kill(pid, SIGKILL);
      waitpid(pid, &status, 0);
      result.exitCode = -1;
   } else {
      waitpid(pid, &status, 0);
      result.exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
   }

   return result;
}

} // namespace app
