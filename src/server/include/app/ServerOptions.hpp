#pragma once

#include <cstddef>
#include <string>

namespace app {

//------------------------------------------------------------------------------
// Opcoes de linha de comando do 'server' (a camada HTTP -- nao linka
// mixr_dep, ver README.md).
//
//   -port <N>              porta de escuta (default 8080)
//   -bind <endereco>        interface de bind (default 0.0.0.0)
//   -runner <caminho>       caminho do binario sim-runner; se omitido,
//                           resolvido como o irmao de '/proc/self/exe'
//                           (mesma tecnica de app/Respawn.hpp)
//   -max-concurrent <N>     quantas simulacoes rodam em paralelo antes de
//                           devolver 503 (default 4)
//   -timeout <segundos>     tempo maximo por simulacao antes de SIGKILL +
//                           504 (default 30)
//------------------------------------------------------------------------------
struct ServerOptions
{
   int port{8080};
   std::string bindAddress{"0.0.0.0"};
   std::string runnerPath;   // vazio = resolver em runtime

   int maxConcurrentSims{4};
   int subprocessTimeoutSec{30};

   // Limites do corpo/parametros da requisicao -- ver ScenarioUpload.hpp.
   std::size_t maxBodyBytes{256 * 1024};
   long defaultFrames{600};
   long maxFrames{6000};
   int maxThreads{16};
};

ServerOptions parseServerCommandLine(int argc, char* argv[]);

} // namespace app
