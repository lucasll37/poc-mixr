//
// server -- a camada HTTP da API REST de simulacao.
//
// NAO linka mixr_dep. Toda requisicao POST /simulate vira um processo
// sim-runner, um por requisicao (ver app/HttpServer.hpp, app/Subprocess.hpp
// e src/server/README.md para o contrato completo e o porque desta
// arquitetura).
//

#include "app/HttpServer.hpp"
#include "app/ServerOptions.hpp"

int main(int argc, char* argv[])
{
   const app::ServerOptions opts{app::parseServerCommandLine(argc, argv)};
   return app::runHttpServer(opts);
}
