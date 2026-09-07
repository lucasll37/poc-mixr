# `libs/xinfer` — inferência ONNX dentro do frame

Roda um `.onnx` de dentro da decisão. Três funções: `open()`, `shape()`, `run()`.
O tipo do ONNX Runtime não aparece na interface — ver o "porquê" no cabeçalho de
[`Infer.hpp`](Infer.hpp).

## Como se usar

Ninguém declara `xinfer` num `.edl` diretamente — quem usa é um nó de
BehaviorTree, `( OnnxPolicy )`, dentro do `.xml` que o `treeFile:` do
`( BtBehavior )` aponta. No cenário `src/poc/onnx-policy`:

```edl
behavior: ( BtBehavior
   ...
   treeFile: "./src/poc/onnx-policy/configs/flight_tree_onnx.xml"
   ... )
```

E dentro desse XML (`src/poc/onnx-policy/configs/flight_tree_onnx.xml`), o nó que de
fato chama esta lib — o único porto obrigatório é `model`, caminho relativo à
raiz do repositório como todo caminho de `configs:`/`data:` das pocs:

```xml
<Fallback name="root">
   <OnnxPolicy model="./src/poc/onnx-policy/configs/policy_barrier.onnx"
               normalized="true"
               label="ONNX"/>
   <Patrol/>
</Fallback>
```

O `Patrol` depois é a degradação: se o `.onnx` não abrir ou tiver forma errada,
`OnnxPolicy` devolve `FAILURE` e o `Fallback` cai nele.

Quem de fato chama a API é o C++ por trás desse nó
(`models/player/A4/src/bt/nodes/OnnxPolicyAction.cpp`) — sempre as três funções,
nesta ordem:

```cpp
#include "xinfer/Infer.hpp"

// 1) abrir -- preguicoso e cacheado por caminho: chamar de novo com o MESMO
//    caminho devolve o id ja carregado, nao reabre a sessao
mixr::xinfer::ModelId modelId = mixr::xinfer::open(caminho);
if (modelId == 0) { /* .onnx ausente/invalido -- degradar, nao abortar */ }

// 2) conferir a forma ANTES de rodar -- o contrato desta poc e 28 entradas,
//    3 saidas (xrlbridge/ObservationFields.hpp)
int nIn{}, nOut{};
mixr::xinfer::shape(modelId, nIn, nOut);

// 3) inferir -- devolve quantos floats foram escritos, ou NEGATIVO em falha
float entrada[28]{ /* domain::WorldView, na ordem canonica */ };
float saida[3]{};
const int escritos = mixr::xinfer::run(modelId, entrada, 28, saida, 3);
if (escritos != 3) { /* falha -- o no devolve FAILURE, o Fallback decide */ }
```

`xinfer` não sabe o que os números significam — quem monta `entrada[]` na ordem
certa e interpreta `saida[]` como rumo/altitude/velocidade é o próprio
`OnnxPolicyAction::tick()`. Teste: `tests/domain/test_xinfer.cpp`
(suíte `domain`) — degradação (arquivo vazio/inexistente/inválido nunca aborta,
falha não entra no cache) e o caminho feliz contra o `.onnx` de verdade que o
modelo instala (forma do contrato, mesmo id por caminho, determinismo bit a bit
em 1000 repetições e entre 4 threads concorrentes).

## Por que é uma `shared_library()` do SDK

O mesmo argumento de [`xboard`](../xboard/Board.hpp) e [`xrlbridge`](../xrlbridge/RLBridge.hpp),
mais uma razão própria: o ONNX Runtime em Debug pesa **576 MB** depois de linkado, e
`models/player/A4/meson.build` gera **quatro** artefatos do mesmo `model_sources`. Dentro do plugin
seriam quatro cópias, recopiadas por `sync-plugins` a cada `make install`. Aqui é uma.

## Números medidos

| | |
|---|---|
| Inferência (MLP 28→64→64→3, 6.211 parâmetros) | **50,1 µs** média, p99 71,5 µs — 0,25% do frame de 20 ms |
| Mesma sessão, 4 threads concorrentes | 55,1 µs média, p99 88,8 µs |
| Com as opções *default* (sem fixar `intra_op`) | **mais lento**: 76,0 µs (1 thread), 96,8 µs (4) |
| `Ort::Session` ctor | 51 ms a frio, 8–9 ms depois — daí o cache |
| Determinismo | 2000 inferências e 1/2/4 threads: saída **byte-idêntica** |
| Tamanho da `.so` | 576 MB → **39 MB** com `--strip-debug` |
| Símbolos fortes exportados pelo plugin que a consome | **1** (`mixr_plugin_v1`) — o gate passa |

## Armadilhas confirmadas

1. **`-Wl,--exclude-libs,ALL` é obrigatória.** `gnu_symbol_visibility: 'hidden'` **não** se aplica
   a objetos vindos de um `.a` — a mesma razão pela qual `models/player/A4/meson.build` precisa dela
   para a BehaviorTree.CPP.
2. **`xinfer_dep` não propaga `onnx_dep`.** É isso que mantém o ORT privado. Acrescentar
   `dependencies: [onnx_dep]` ao `declare_dependency()` vazaria o ORT para todo consumidor e
   obrigaria o `poc-mixr-sdk.pc` a declarar `Requires: onnxruntime`.
3. **Fixar `intra_op_num_threads=1` é mais RÁPIDO, não só mais determinístico.** A coordenação do
   pool custa mais que o ganho num modelo deste tamanho.
4. **`run()` solta o mutex antes de inferir.** `Ort::Session::Run` é seguro para chamada
   concorrente; segurar o mutex serializaria as quatro aeronaves sem necessidade.
5. **Falha nunca aborta.** `open()` devolve `0` e loga; o consumidor degrada — mesma política do
   joystick ausente (`libs/xjoystick`) e da árvore que não carrega (`ubf/BtBehavior`).
