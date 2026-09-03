#pragma once

#include "app/ServerOptions.hpp"

namespace app {

//------------------------------------------------------------------------------
// As rotas HTTP -- POST /simulate e GET /health -- e o laco de escuta.
//
// Uma unica questao: traduzir requisicao HTTP em execucao de
// sim-runner e a resposta dele de volta em HTTP. Nao ha MIXR aqui dentro:
// so texto, arquivo e processo (ver Subprocess.hpp/ScenarioUpload.hpp).
//
// Bloqueia rodando o servidor (svr.listen); so retorna se o listen falhar
// ou for interrompido.
//------------------------------------------------------------------------------
int runHttpServer(const ServerOptions& opts);

} // namespace app
