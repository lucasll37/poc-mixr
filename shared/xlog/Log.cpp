#include "xlog/Log.hpp"

#include "mixr/recorder/PrintHandler.hpp"
#include "mixr/base/String.hpp"
#include "mixr/base/safe_ptr.hpp"

#include <chrono>
#include <ctime>
#include <deque>
#include <iomanip>
#include <iostream>
#include <mutex>

namespace mixr {
namespace xlog {

namespace {

std::mutex g_mutex;
bool g_enabled{true};
bool g_consoleEnabled{true};
base::safe_ptr<recorder::PrintHandler> g_sink;

// Buffer circular das ultimas linhas -- ver o "porque" em Log.hpp. deque
// e nao vector: a operacao dominante e "empurra no fim, descarta do
// inicio", O(1) nos dois lados, sem realocar o miolo.
std::deque<Entry> g_entries;
std::uint64_t g_seq{};


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

const char* levelName(const Level level)
{
   switch (level) {
      case Level::DEBUG:   return "DEBUG";
      case Level::INFO:    return "INFO";
      case Level::WARNING: return "WARNING";
      case Level::ERROR:   return "ERROR";
   }
   return "?";
}

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

void setConsoleEnabled(const bool enabled)
{
   std::lock_guard<std::mutex> lock(g_mutex);
   g_consoleEnabled = enabled;
}

std::uint64_t lastSeq()
{
   std::lock_guard<std::mutex> lock(g_mutex);
   return g_seq;
}

std::vector<Entry> snapshot()
{
   std::lock_guard<std::mutex> lock(g_mutex);
   return std::vector<Entry>(g_entries.begin(), g_entries.end());
}

Stream::Stream(const Level level) : level(level)
{
}

Stream::~Stream()
{
   std::lock_guard<std::mutex> lock(g_mutex);
   if (!g_enabled) return;

   const std::string stamp{timestamp()};
   const std::string text{buffer.str()};
   const std::string line{"[" + stamp + "] [" + levelName(level) + "] " + text};

   if (g_consoleEnabled) std::cout << line << std::endl;
   if (g_sink != nullptr) g_sink->printToOutput(line.c_str());

   // O buffer guarda os campos SEPARADOS (nao a linha ja formatada) --
   // quem exibe quer colorir por nivel e alinhar o carimbo em coluna
   // propria; refatiar a string formatada de volta seria trabalho a toa.
   g_seq += 1;
   g_entries.push_back(Entry{g_seq, level, stamp, text});
   while (g_entries.size() > kMemoryCapacity) g_entries.pop_front();
}

} // namespace xlog
} // namespace mixr
