#ifndef __xpyembed_PyEmbed_H__
#define __xpyembed_PyEmbed_H__

#include <string>

namespace mixr {
namespace xpyembed {

// O alvo compila com gnu_symbol_visibility:'hidden'. Sem esta marca a PROPRIA
// API fica invisivel e o consumidor falha no link -- 'nm -D' devolve ZERO
// simbolos fortes. Mesma armadilha de shared/xinfer/Infer.hpp; ela pega duas
// vezes porque o sintoma ('nm -D' vazio) so aparece se alguem for olhar.
#define XPYEMBED_API __attribute__((visibility("default")))

//------------------------------------------------------------------------------
// PYTHON EMBARCADO -- rodar um script de decisao de dentro do frame.
//
// Uma unica questao: dado um script e um vetor de observacao, devolver um
// vetor de comando. Toda chamada a API C do CPython do repositorio inteiro
// mora neste arquivo e no .cpp dele.
//
// PARA QUE SERVE: prototipar comportamento. Escrever uma regra nova em Python,
// ver o efeito em segundos, e so depois -- se ela se provar -- reescrever em
// C++. E a tensao registrada em TODO.md: "testar em python para migrar para
// mixr/cpp facilita a prototipagem mas abre margem para desafios de
// integracao". Esta lib e a resposta a esses desafios.
//
//------------------------------------------------------------------------------
// POR QUE libpython E ALCANCADA POR dlopen, E NAO LINKADA
//
// Tres razoes, as tres MEDIDAS:
//
//   1. O /usr/bin/python3 do Ubuntu NAO linka libpython dinamicamente -- ela e
//      ESTATICA dentro do executavel. Um DT_NEEDED de libpython3.12.so faria o
//      processo do src/rl (onde a simulacao roda DENTRO do Python) carregar um
//      SEGUNDO runtime. Aqui, quando ja ha interpretador, nada e carregado: os
//      simbolos sao resolvidos por RTLD_DEFAULT.
//
//   2. As extensoes C do Python (numpy, ctypes, _socket) SO importam com
//      libpython no escopo GLOBAL do loader. Medido: de dentro de um plugin
//      aberto com RTLD_LOCAL, 'import ctypes' morre com "undefined symbol:
//      PyTuple_Type". O dlopen com RTLD_GLOBAL e o que conserta -- e nenhum
//      DT_NEEDED conseguiria, porque a lib herda o escopo de quem a carregou.
//
//   3. Python vira dependencia OPCIONAL de runtime. Sem Python instalado,
//      isAvailable() devolve false e o no da arvore degrada -- mesma politica
//      do joystick ausente em shared/xjoystick. Com DT_NEEDED, o binario
//      simplesmente nao subiria.
//
// O preco e a tabela de ponteiros de funcao no .cpp. Vale: nenhum main.cpp de
// host precisou de uma linha.
//
//------------------------------------------------------------------------------
// CONCORRENCIA E DETERMINISMO
//
// O GIL serializa as decisoes -- medido: 7,9 us por chamada com 1 thread, 17,6
// com 4. A 4 avioes x 50 Hz isso e 0,35% de duty cycle, entao a serializacao
// nao e o problema. O problema seria ESTADO COMPARTILHADO: a ordem em que as
// threads adquirem o GIL nao e deterministica, entao um script que guarde
// estado em nivel de modulo produziria resultado dependente de ordem, e o
// check-multi-thread quebraria.
//
// Por isso cada (script, playerId) recebe o SEU PROPRIO dicionario de globais.
// Dois avioes rodando o mesmo arquivo nao se enxergam. O que continua
// compartilhado e o sys.modules -- um 'import' traz o MESMO objeto de modulo
// para todos, entao um modulo importado com estado mutavel ainda e um buraco.
// Esta documentado, nao resolvido: resolver exigiria um subinterpretador por
// aviao, e o pybind11 desta versao nao os suporta.
//------------------------------------------------------------------------------

// Identificador opaco de um script carregado. ZERO = invalido.
using ScriptId = int;

// Ha um interpretador Python utilizavel neste processo? False quando a
// libpython nao foi encontrada -- o consumidor degrada, nao aborta.
XPYEMBED_API bool isAvailable();

// Carrega um script e devolve o id, ou 0 em falha. O script tem de definir
//
//     def decide(obs):        # obs: lista de 28 floats, ordem canonica
//         return (heading_deg, altitude_m, speed_kts)
//
// Preguicoso e unico por caminho.
XPYEMBED_API ScriptId loadScript(const std::string& path);

// Chama decide() com a observacao, no dicionario de globais DESTE playerId.
// Devolve false em qualquer falha (script nao carregado, excecao no Python,
// retorno com forma errada) -- sem lancar, sem abortar.
XPYEMBED_API bool decide(ScriptId id, int playerId, const double* obs, int nObs, double* cmd, int nCmd);

} // namespace xpyembed
} // namespace mixr

#endif
