# `shared/xpyembed` — Python embarcado dentro do frame

Roda um script de decisão de dentro do `genAction()`. Três funções:
`isAvailable()`, `loadScript()`, `decide()`. **Toda** chamada à API C do CPython do repositório
inteiro mora em [`PyEmbed.cpp`](PyEmbed.cpp).

Para que serve: **prototipagem**. Escrever uma regra em Python, ver o efeito em segundos, e só
depois — se ela se provar — reescrever em C++. É a tensão registrada em `TODO.md` ("testar em
python para migrar para mixr/cpp facilita a prototipagem mas abre margem para desafios de
integração") com os desafios de integração resolvidos.

## Por que `libpython` é alcançada por `dlopen`, e não linkada

Três razões, as três medidas:

1. **O `/usr/bin/python3` do Ubuntu tem `libpython` estática.** Um `DT_NEEDED` de
   `libpython3.12.so` faria o processo do `src/rl` — onde a simulação roda *dentro* do Python —
   carregar um **segundo** runtime. Aqui, quando já há interpretador, nada é carregado: os símbolos
   vêm por `RTLD_DEFAULT`.
2. **As extensões C só importam com `libpython` no escopo global.** De dentro de um plugin aberto
   com `RTLD_LOCAL`, `import ctypes` morre com `undefined symbol: PyTuple_Type` e `import numpy`
   com `PyExc_ImportError`. O `dlopen(..., RTLD_GLOBAL)` conserta; nenhum `DT_NEEDED` conseguiria,
   porque a lib herda o escopo de quem a carregou.
3. **Python vira dependência opcional de runtime.** Sem Python, `isAvailable()` devolve `false` e o
   nó degrada — mesma política do joystick ausente. Com `DT_NEEDED`, o binário não subiria.

O preço é a tabela de ponteiros de função. Vale: **nenhum `main.cpp` de host precisou de uma linha.**

## Números medidos

| | |
|---|---|
| Latência por chamada, 1 thread | **7,9 µs** |
| 2 threads / 4 threads | 17,6 / 17,8 µs (o GIL serializa) |
| Saturação | ~56 mil chamadas/s |
| Duty cycle real (4 aeronaves × 50 Hz) | **0,35%** do frame |
| `import numpy` de dentro de plugin `RTLD_LOCAL` | falha sem `RTLD_GLOBAL`, **funciona** com |
| `DT_NEEDED` de libpython em `libxpyembed.so` | **nenhum** |

## Determinismo

O GIL serializa, mas a **ordem** de aquisição não é determinística. O que mantém
`make check-multi-thread` verde é cada `(script, chave)` ter o **seu próprio dicionário de globais**
— dois aviões rodando o mesmo arquivo não se enxergam. Travado por
`tests/domain/test_xpyembed.cpp::QuatroThreadsComChavesDistintasNaoSeMisturam`.

**Limite conhecido:** `sys.modules` continua compartilhado. Um `import` traz o mesmo objeto de
módulo para todos, então um módulo importado com estado mutável ainda é um buraco. Resolver exigiria
um subinterpretador por aeronave, e o pybind11 desta versão não os suporta.

## Armadilhas

1. **`gnu_symbol_visibility: 'hidden'` esconde a própria API.** Sem `XPYEMBED_API` nas declarações,
   `nm -D` devolve **zero** símbolos fortes e o consumidor falha no link. A mesma armadilha pegou
   duas vezes (aqui e em `shared/xinfer`), porque o sintoma só aparece se alguém for olhar.
2. **`Py_InitializeEx` segura o GIL.** Sem `PyEval_SaveThread()` logo depois, a primeira
   `PyGILState_Ensure()` de outra thread trava para sempre.
3. **`PyList_SetItem` e `PyTuple_SetItem` roubam a referência.** Dar `Py_DecRef` no item depois de
   inseri-lo é um *double free*.
4. **O `g_mutex` não cobre a chamada, só a carga.** Quem serializa a execução é o GIL; segurar os
   dois criaria uma segunda ordem de aquisição, e com ela a chance de deadlock.
5. **`Py_file_input` é 257.** Vem de `Python.h`, que deliberadamente não é incluído — está fixado
   como constante, com o comentário explicando.
