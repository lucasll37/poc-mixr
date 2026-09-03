#pragma once

#include <string>
#include <vector>

namespace app {

//------------------------------------------------------------------------------
// Spawn de UM processo-filho, sem shell nenhum no meio (execv com argv
// explicito -- nunca system()/popen(): o corpo da requisicao HTTP acaba
// virando um argumento de linha de comando, um caminho de arquivo, e
// passar por um shell abriria injecao a partir de dado do cliente).
//
// Captura stdout/stderr por pipe, com um teto de tempo: se o filho nao
// terminar dentro de 'timeoutSec', leva SIGKILL e 'timedOut' vem true.
//
// E o unico lugar do 'server' que mexe com processo/pipe -- HttpServer.cpp
// so chama isto e interpreta o resultado (exitCode/timedOut/stdoutText/
// stderrText).
//------------------------------------------------------------------------------
struct SubprocessResult
{
   bool timedOut{};
   int exitCode{-1};    // -1 se nao terminou normalmente (timeout, sinal, spawn falhou)
   std::string stdoutText;
   std::string stderrText;
};

SubprocessResult runSubprocess(const std::string& exePath, const std::vector<std::string>& args,
                               int timeoutSec);

} // namespace app
