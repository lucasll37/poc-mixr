#pragma once

#include <string>
#include <vector>

namespace app {

//------------------------------------------------------------------------------
// "Trocar de cenario" e "reiniciar" sao um REEXEC de si mesmo -- nunca uma
// segunda Station no mesmo processo.
//
// O motivo mora no proprio pipeline: app::buildStation() (StationBuilder.cpp)
// chama xplugin::setBuiltinFactory() + edl_parser() + xplugin::seal() UMA VEZ
// por processo -- em NENHUM lugar do repositorio esse caminho e chamado uma
// segunda vez, e shared/xplugin/README.md documenta que plugins nao tem
// descarga nem hot-reload em processo vivo. Reconstruir uma Station por cima
// disso pisaria em terreno nunca exercitado. execv() e o caminho 100%
// testado -- e o que ja acontece toda vez que alguem roda o binario de novo
// -- so que sub-segundo e sem o usuario precisar digitar nada.
//
// Resolve o proprio caminho via /proc/self/exe (Linux) em vez de reusar
// argv[0]: argv[0] pode ser relativo ("./dashboard") ou vir de um PATH
// resolvido pelo shell -- /proc/self/exe e sempre absoluto e sempre certo.
//
// NAO RETORNA em caso de sucesso (execv substitui a imagem do processo). Em
// caso de falha (readlink ou execv), imprime o erro e termina o processo --
// tambem nao retorna, so que com falha.
//------------------------------------------------------------------------------
[[noreturn]] void respawnSelf(const std::vector<std::string>& args);

} // namespace app
