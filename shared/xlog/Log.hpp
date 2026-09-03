#ifndef __xlog_Log_H__
#define __xlog_Log_H__

#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

namespace mixr {
namespace xlog {

//------------------------------------------------------------------------------
// Sistema de log com nivel, sintaxe de stream e persistencia em arquivo --
// compartilhado entre os subprojetos (mesmo padrao 'shared/x<nome>').
//
// Uso:
//    LOG(WARNING) << "algo aconteceu: " << valor;
//
// POR QUE NAO E O mixr::recorder (DataRecorder/OutputHandler/REID/protobuf):
// o schema DataRecord.proto nao tem nenhum campo de texto livre em nenhuma
// mensagem por-evento (nem o MarkerMsg, que so carrega dois uint32), e o
// unico ponto de entrada publico do gravador, AbstractDataRecorder::
// recordData(id, pObjects[4], values[4]), nao aceita string.
//
// O QUE ESTE ARQUIVO REAPROVEITA DO recorder, entao: mixr::recorder::
// PrintHandler (base de TabPrinter etc.) -- mas por FORA do pipeline
// recordData()/REID/protobuf. printToOutput(const char*) escreve direto
// num std::ofstream que ele mesmo abre (configurado por setFilename(), um
// metodo publico comum, nao so slot de EDL) -- nunca passa por
// processRecordImp()/DataRecordHandle, entao nunca esbarra no schema
// fechado. Ja e dependencia transitiva de mixr_dep (mixr-recorder no
// Requires: do mixr.pc, a mesma lib que o shared/xtacview linka) -- nenhuma
// dependencia nova.
//------------------------------------------------------------------------------

enum class Level { DEBUG, INFO, WARNING, ERROR };

// Abre (ou tenta abrir) o arquivo de log. Chamar uma vez, cedo no main.cpp
// de cada poc -- path relativo a raiz do repo, mesma convencao de
// configs/data (ver CLAUDE.md). O diretorio TEM que existir no disco: como
// o PrintHandler nativo, esta funcao nao cria diretorios.
void init(const std::string& filePath);

// '-deterministic' desliga: linhas de log carregam timestamp de parede,
// fora do modo comparavel. Desliga TUDO -- console, arquivo e o buffer em
// memoria abaixo.
void setLoggingEnabled(bool enabled);

// Liga/desliga SO a copia no console (std::cout), sem afetar arquivo nem
// buffer. Existe para quem e dono do terminal: o ./app roda um painel
// FTXUI em tela cheia, e uma linha escrita direto em stdout no meio disso
// suja o desenho (o FTXUI nao sabe que alguem escreveu por baixo dele e
// nao redesenha aquela regiao). Com o console desligado a linha continua
// indo pro arquivo e pro buffer -- que e de onde a aba "Log" a le.
void setConsoleEnabled(bool enabled);

//------------------------------------------------------------------------------
// BUFFER EM MEMORIA -- as ultimas kMemoryCapacity linhas, para quem quer
// EXIBIR o log em vez de so grava-lo (a aba "Log" do ./app).
//
// Por que aqui e nao um tail do arquivo: (a) o arquivo e escrito por um
// mixr::recorder::PrintHandler com flush proprio, entao reler o que
// acabou de ser escrito e uma corrida contra o buffer do ofstream; (b)
// esta lib ja e o ponto por onde TODA linha passa, com o mutex que ja
// serializa os escritores; e (c) -- o motivo estrutural -- 'xlog' e uma
// shared_library() (ver o comentario em shared/xlog/meson.build), entao ha
// UMA copia so no processo: o LOG(...) do MODELO, que mora num .so aberto
// por dlopen, cai no MESMO buffer que o do host. A aba mostra os dois sem
// nenhuma ponte extra.
//------------------------------------------------------------------------------

struct Entry {
   std::uint64_t seq{};             // 1 na primeira linha do processo, sempre crescente
   Level level{Level::INFO};
   std::string time;                // "HH:MM:SS.mmm", o mesmo carimbo da linha gravada
   std::string text;                // so a mensagem, sem carimbo nem nivel
};

// Quantas linhas o buffer guarda. Passou disso, a mais antiga sai (por
// isso 'seq' e util: a primeira entrada do snapshot diz quantas ja
// escorreram).
const std::size_t kMemoryCapacity{500};

// 'seq' da linha mais recente (0 se nada foi logado ainda) -- barato,
// serve para o chamador so copiar o buffer quando algo mudou de fato, em
// vez de a cada redesenho.
std::uint64_t lastSeq();

// Copia do buffer, do mais ANTIGO para o mais NOVO.
std::vector<Entry> snapshot();

const char* levelName(Level level);

// Objeto temporario por tras da macro LOG(...) -- nao use diretamente.
// RAII: acumula em operator<< e escreve tudo de uma vez no destrutor, o
// que permite 'LOG(WARNING) << a << b;' ser uma expressao so.
class Stream {
public:
   explicit Stream(Level level);
   ~Stream();

   Stream(const Stream&) = delete;
   Stream& operator=(const Stream&) = delete;

   template <typename T>
   Stream& operator<<(const T& value)
   {
      buffer << value;
      return *this;
   }

private:
   Level level;
   std::ostringstream buffer;
};

} // namespace xlog
} // namespace mixr

// LOG(WARNING) << "texto" << x;   -- DEBUG / INFO / WARNING / ERROR
//
// Level e um enum class (nao #define soltos) de proposito: ERROR/DEBUG sao
// nomes de macro classicos em outros contextos (ex.: wingdi.h no Windows).
// Irrelevante aqui -- projeto e Linux-only e nao ha -DDEBUG no build -- mas
// o desenho ja nasce sem essa armadilha.
#define LOG(level) mixr::xlog::Stream(mixr::xlog::Level::level)

#endif
