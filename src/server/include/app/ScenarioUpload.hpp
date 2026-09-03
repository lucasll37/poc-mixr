#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace app {

//------------------------------------------------------------------------------
// Validacao HEURISTICA do corpo enviado em POST /simulate -- nao e um
// parser EDL (isso fica por conta do edl_parser dentro do sim-runner, que
// e quem tem a palavra final). O que checa aqui:
//
//   * tamanho (nao-vazio, <= maxBytes)
//   * texto plausivel (sem byte NUL -- corpo binario e claramente errado)
//   * ausencia dos tokens PluginLoader/PluginModule/networks:/dataRecorder:
//     -- ver o comentario grande em src/server/README.md e no cabecalho de
//     scenario_prefix.epp.in para o porque de cada um: o sim-runner ja
//     injeta o PluginLoader dele (bloqueia dlopen arbitrario a partir de um
//     corpo de terceiro), 'networks:' abriria socket DIS (nao-hermetico) e
//     'dataRecorder:' colidiria de porta Tacview entre requisicoes
//     concorrentes (nenhuma delas tem porta 'own', sao fixas no EDL).
//------------------------------------------------------------------------------
struct UploadValidation
{
   bool valid{};
   std::vector<std::string> errors;
};

UploadValidation validateScenarioBody(const std::string& body, std::size_t maxBytes);

} // namespace app
