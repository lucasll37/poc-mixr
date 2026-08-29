#include "xlog/Log.hpp"

#include "mixr/recorder/PrintHandler.hpp"
#include "mixr/base/String.hpp"
#include "mixr/base/safe_ptr.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>

namespace mixr {
namespace xlog {

namespace {

std::mutex g_mutex;
bool g_enabled{true};
base::safe_ptr<recorder::PrintHandler> g_sink;

const char* levelTag(const Level level)
{
   switch (level) {
      case Level::DEBUG:   return "DEBUG";
      case Level::INFO:    return "INFO";
      case Level::WARNING: return "WARNING";
      case Level::ERROR:   return "ERROR";
   }
   return "?";
}

std::string timestamp()
{
   using namespace std::chrono;
   const auto now{system_clock::now()};
   const std::time_t t{system_clock::to_time_t(now)};
   const auto ms{duration_cast<milliseconds>(now.time_since_epoch()) % 1000};

   std::tm tm{};
   localtime_r(&t, &tm);

   std::ostringstream oss;
   oss << std::put_time(&tm, "%H:%M:%S")
       << '.' << std::setfill('0') << std::setw(3) << ms.count();
   return oss.str();
}

} // namespace

void init(const std::string& filePath)
{
   std::lock_guard<std::mutex> lock(g_mutex);

   // 'new ..., false' via set(): o objeto ja nasce com refCount 1
   // (STANDARD_CONSTRUCTOR) -- refar aqui so vazaria uma referencia que
   // nada mais solta.
   const auto sink = new recorder::PrintHandler();
   const base::String name(filePath.c_str());
   sink->setFilename(&name);

   // openFile() e preguicoso por padrao (PrintHandler::printToOutput abre
   // no primeiro uso) -- chamar aqui so para falhar cedo, no boot, se o
   // diretorio nao existir, em vez de falhar em silencio no primeiro
   // LOG(...) (mesma armadilha que hoje afeta o TacviewOutput sem
   // data/recordings/ no disco).
   sink->openFile();

   g_sink.set(sink, false);
}

void setLoggingEnabled(const bool enabled)
{
   std::lock_guard<std::mutex> lock(g_mutex);
   g_enabled = enabled;
}

Stream::Stream(const Level level) : level(level)
{
}

Stream::~Stream()
{
   std::lock_guard<std::mutex> lock(g_mutex);
   if (!g_enabled) return;

   const std::string line{"[" + timestamp() + "] [" + levelTag(level) + "] " + buffer.str()};

   std::cout << line << std::endl;
   if (g_sink != nullptr) g_sink->printToOutput(line.c_str());
}

} // namespace xlog
} // namespace mixr
