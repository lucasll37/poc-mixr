#ifndef __xlog_Log_H__
#define __xlog_Log_H__

#include <sstream>
#include <string>

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
// fora do modo comparavel.
void setLoggingEnabled(bool enabled);

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
