#pragma once

#include <string>

namespace app {

//------------------------------------------------------------------------------
// Monta o .epp final: scenario_prefix.epp.in + o corpo enviado pelo cliente
// (so o conteudo de 'players: {}') + scenario_suffix.epp.in, substituindo
// @NUM_TC_THREADS@ -- mesma tecnica de substituicao literal de string, ANTES
// do edl_parser ver o arquivo, que app/src/app/ScenarioTemplate.cpp usa para
// o mesmo token (mas essa aqui e uma implementacao propria: o sim-runner
// concatena TRES arquivos, nao um so, e nao precisa do mecanismo
// '@include:' do app -- so o placeholder de threads).
//
// Encerra o processo se prefixo/corpo/sufixo nao puderem ser lidos: sem
// cenario nao ha simulacao.
//------------------------------------------------------------------------------
int assembleScenario(const std::string& prefixPath, const std::string& bodyPath,
                     const std::string& suffixPath, const std::string& outPath,
                     int threadsOverride);

} // namespace app
