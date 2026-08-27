#pragma once

#include <string>

namespace app {

//------------------------------------------------------------------------------
// Opcoes de linha de comando desta poc.
//
// Uma unica questao: traduzir argv para uma estrutura. Nada aqui abre
// arquivo, constroi Station nem decide o que fazer com as opcoes -- isso e
// do main.cpp e dos modulos que ele orquestra.
//
//   -f <arquivo>         cenario alternativo (o .epp.in, nao o gerado)
//   -threads <N>         numTcThreads do pool nativo de tempo critico
//   -deterministic <N>   roda N frames de passo fixo e sai (sem tempo real)
//------------------------------------------------------------------------------
struct Options
{
   std::string templatePath;
   std::string generatedPath;
   long deterministicFrames{};
   int threadsOverride{};

   bool isDeterministic() const   { return deterministicFrames > 0; }
};

// Aplica sobre 'defaults' o que vier em argv; argumentos desconhecidos sao
// ignorados (mesma tolerancia dos exemplos do framework).
Options parseCommandLine(int argc, char* argv[], const Options& defaults);

} // namespace app
