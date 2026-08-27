#ifndef __xnative_Log_H__
#define __xnative_Log_H__

#include <string>

namespace mixr {
namespace xnative {

//------------------------------------------------------------------------------
// Log de console seguro entre threads.
//
// Uma unica questao: std::cout nao e sincronizado, e aqui varios players
// escrevem de threads de tempo critico diferentes -- sem um mutex as linhas
// se entrelacam. Usar apenas em eventos raros (troca de estado da arvore,
// falha ao carregar a arvore), nunca a cada frame.
//------------------------------------------------------------------------------

void logLine(const std::string& line);

// O modo '-deterministic' desliga o log: essas linhas carregam o numero da
// thread, que depende do escalonador e nao do estado da simulacao.
void setLoggingEnabled(bool enabled);

} // namespace xnative
} // namespace mixr

#endif
