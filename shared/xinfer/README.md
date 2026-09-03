# `shared/xinfer` — inferência ONNX dentro do frame

Roda um `.onnx` de dentro da decisão. Três funções: `open()`, `shape()`, `run()`.
O tipo do ONNX Runtime não aparece na interface — ver o "porquê" no cabeçalho de
[`Infer.hpp`](Infer.hpp).

## Por que é uma `shared_library()` do SDK

O mesmo argumento de [`xboard`](../xboard/Board.hpp) e [`xrlbridge`](../xrlbridge/RLBridge.hpp),
mais uma razão própria: o ONNX Runtime em Debug pesa **576 MB** depois de linkado, e
`models/flight/meson.build` gera **quatro** artefatos do mesmo `model_sources`. Dentro do plugin
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

## Armadilhas

1. **`-Wl,--exclude-libs,ALL` é obrigatória.** `gnu_symbol_visibility: 'hidden'` **não** se aplica
   a objetos vindos de um `.a` — a mesma razão pela qual `models/flight/meson.build` precisa dela
   para a BehaviorTree.CPP.
2. **`xinfer_dep` não propaga `onnx_dep`.** É isso que mantém o ORT privado. Acrescentar
   `dependencies: [onnx_dep]` ao `declare_dependency()` vazaria o ORT para todo consumidor e
   obrigaria o `poc-mixr-sdk.pc` a declarar `Requires: onnxruntime`.
3. **Fixar `intra_op_num_threads=1` é mais RÁPIDO, não só mais determinístico.** A coordenação do
   pool custa mais que o ganho num modelo deste tamanho.
4. **`run()` solta o mutex antes de inferir.** `Ort::Session::Run` é seguro para chamada
   concorrente; segurar o mutex serializaria as quatro aeronaves sem necessidade.
5. **Falha nunca aborta.** `open()` devolve `0` e loga; o consumidor degrada — mesma política do
   joystick ausente (`shared/xjoystick`) e da árvore que não carrega (`ubf/BtBehavior`).
