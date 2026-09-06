#pragma once

#include <string>

namespace app {

//------------------------------------------------------------------------------
// A parte SEM FTXUI da aba "EDL" (F7): editar um cenario EM MEMORIA e validar
// o resultado contra o mesmo oraculo (`edlcheck`, ver app/src/edlcheck_main.cpp)
// que ja existia para o editor grafico web (src/ui/edl_builder.jsx).
//
// "Sem persistir no arquivo real": o texto editado NUNCA e escrito no
// '.edl.in' de origem nem no '.generated.edl' que o processo carregou --
// os dois unicos lugares que main.cpp/ScenarioTemplate le na carga normal.
// Ele so vai para editedScenarioPath(), um arquivo de TRABALHO a parte
// (gitignored, ver app/data/edl_editor/.gitkeep), escrito de novo a cada
// "Validar"/"Rodar". "Rodar" nunca constroi uma segunda Station no mesmo
// processo -- e sempre um REEXEC com '-f' (ver DashboardExit::RunEdited em
// app/DashboardLoop.hpp e o "porque" em app/Respawn.hpp), a mesma regra que
// ja vale para "Carregar"/"Reiniciar".
//------------------------------------------------------------------------------

// Sempre o MESMO caminho -- um unico editor por vez, mesma premissa ja usada
// pelo '.generated.edl' de cada cenario (tests/meson.build ja serializa os
// testes que rodam um binario por isso, 'is_parallel: false').
const std::string& editedScenarioPath();

// Le o arquivo inteiro para string; "" se nao existir/nao abrir -- o editor
// so comeca vazio (uma aba a menos com conteudo), ao contrario de
// ScenarioTemplate::readFileOrDie(), que e fatal (sem cenario nao ha
// simulacao; aqui e so a aba do editor).
std::string readEdlFileOrEmpty(const std::string& path);

// Escreve 'text' em 'path', criando o diretorio-pai se faltar. Devolve false
// em falha (disco cheio, permissao) -- nunca chamado sobre o '.edl.in'/
// '.generated.edl' do cenario, so sobre editedScenarioPath().
bool writeEdlFile(const std::string& path, const std::string& text);

struct EdlValidationResult
{
   bool ok{};
   std::string message;   // stdout+stderr combinados do 'edlcheck', sem \n final
};

// Roda '<edlcheckBinaryPath> <edlFilePath>' e interpreta o exit code (ver o
// cabecalho de app/src/edlcheck_main.cpp: 0 = OK, qualquer outro = invalido).
// 'edlcheckBinaryPath' e parametro -- nao resolvido aqui dentro -- para dar
// pra testar sem depender de /proc/self/exe nem do binario 'edlcheck' de
// verdade (ver tests/app/test_edl_editor_state.cpp).
EdlValidationResult runEdlCheck(const std::string& edlcheckBinaryPath, const std::string& edlFilePath);

// Caminho do binario 'edlcheck', irmao do proprio executavel em execucao --
// mesmo truque de /proc/self/exe que app/Respawn.hpp ja usa para se
// reexecutar. Funciona rodando de 'build/app/src/' ou de 'dist/bin/', sem
// hardcodar qual dos dois -- os dois projetos instalam as duas ferramentas
// juntas (ver app/src/meson.build).
std::string edlcheckSiblingPath();

} // namespace app
