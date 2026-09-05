#ifndef __xinfer_Infer_H__
#define __xinfer_Infer_H__

#include <string>

namespace mixr {
namespace xinfer {

// O alvo compila com gnu_symbol_visibility:'hidden' -- e isso e o que esconde
// os simbolos que vem dos .a do ONNX Runtime. Mas 'hidden' nao distingue: sem
// esta marca explicita, a PROPRIA API desta lib fica invisivel tambem, e o
// consumidor falha no link. Confirmado quebrando: 'nm -D' na .so devolvia
// ZERO simbolos fortes.
#define XINFER_API __attribute__((visibility("default")))

//------------------------------------------------------------------------------
// INFERENCIA -- rodar um modelo .onnx de dentro da decisao, no frame.
//
// Uma unica questao: dado um arquivo .onnx e um vetor de float, devolver o
// vetor de saida. Nada aqui sabe o que os numeros SIGNIFICAM -- quem da
// sentido a eles e o no da arvore de comportamento que chama.
//
//------------------------------------------------------------------------------
// POR QUE ISTO E UMA shared_library() DO SDK, E NAO CODIGO DENTRO DO PLUGIN
//
// Mesmo argumento estrutural de shared/xboard/Board.hpp e shared/xrlbridge/
// RLBridge.hpp, aplicado a um terceiro caso, com tres razoes MEDIDAS:
//
//   1. TAMANHO. O ONNX Runtime e um conjunto de .a que, em Debug, pesa 576 MB
//      depois de linkado (537 MB so de debug info). models/player/A4/meson.build
//      gera QUATRO artefatos do mesmo model_sources (flight, flight_tc e as
//      duas variantes de -Dvariants) -- seriam ~2,3 GB recopiados por
//      'sync-plugins' a cada 'make install'. Aqui e uma copia so, e o plugin
//      volta a ter o tamanho do codigo dele.
//
//   2. O CONTRATO DO PLUGIN. models/README.md Sec2.1 limita as 'dependencies:'
//      de um modelo a mixr_dep + sdk_dep + behavior_tree_dep. Promover a peca
//      necessaria a shared_library do SDK e exatamente a saida que
//      shared/xplugin/README.md ja aponta como a honesta.
//
//   3. HIGIENE DE SIMBOLO. tests/plugin/check_plugin_symbol.sh exige que o
//      plugin exporte UM simbolo forte. Com o ORT aqui dentro, o plugin nem
//      chega perto do problema: ele linka '-lxinfer' e nunca ve header nem
//      biblioteca do ORT. Medido: o NEEDED do plugin fica so libxinfer.so.
//
// O tipo do ONNX Runtime NAO aparece nesta interface, de proposito -- e o
// mesmo movimento de xrlbridge, que define a sua propria Observation em vez
// de reusar domain::WorldView. Trocar o motor de inferencia um dia nao toca
// em nenhum consumidor.
//
//------------------------------------------------------------------------------
// CONCORRENCIA: open() e serializada por um mutex (a carga e cara e unica --
// 51 ms a frio, 8-9 ms depois; quatro avioes carregando ao mesmo tempo no
// primeiro frame nao cabem em 20 ms). run() toma o mutex apenas para achar o
// modelo e o SOLTA antes de inferir: Ort::Session::Run e seguro para chamada
// concorrente -- medido com 4 threads, saida byte-identica a de 1 thread.
//
// DETERMINISMO: a sessao e criada com intra-op e inter-op em 1 thread e
// execucao sequencial. Nao e so pelo determinismo -- e MAIS RAPIDO neste
// tamanho de modelo (50,1 us contra 76,0 us com as opcoes default): a
// coordenacao do pool custa mais que o ganho.
//------------------------------------------------------------------------------

// Identificador opaco de um modelo carregado. ZERO significa invalido -- e o
// que open() devolve em qualquer falha, e o que todo consumidor tem de tratar
// (o cenario degrada, nao aborta: mesma politica do joystick ausente em
// shared/xjoystick e da arvore que nao carrega em ubf/BtBehavior).
using ModelId = int;

// Carrega o .onnx do caminho dado, ou devolve o id ja carregado se o MESMO
// caminho ja foi aberto. Preguicosa e unica por caminho. 0 em falha (arquivo
// ausente, .onnx invalido, forma nao suportada) -- com um LOG(ERROR) dizendo
// qual arquivo e por que.
XINFER_API ModelId open(const std::string& path);

// Forma do modelo: quantos floats ele espera na entrada e produz na saida.
// false se o id nao existe. Serve para o consumidor conferir que o .onnx
// casa com o vetor que ele monta, ANTES de rodar.
XINFER_API bool shape(ModelId id, int& nIn, int& nOut);

// Roda uma inferencia. Devolve quantos floats foram escritos em 'out'
// (positivo), ou um valor NEGATIVO em falha. Nao lanca.
XINFER_API int run(ModelId id, const float* in, int nIn, float* out, int nOut);

} // namespace xinfer
} // namespace mixr

#endif
