# `libs/` — bibliotecas compartilhadas entre host e modelo

Uma biblioteca por pasta, no padrão `libs/x<nome>` das bibliotecas de extensão dos exemplos
oficiais do MIXR (`contexts/MIXR-PATTERN-CONTEXT.md`). Cada uma tem o próprio `README.md`: o que
resolve, como usar (com exemplo real — trecho de `.edl`/`.xml` ou chamada C++) e por que é
`shared_library()`, `static_library()` ou header-only.

| lib | o que resolve |
|---|---|
| [`xboard`](xboard/README.md) | quadro de leitura host↔modelo (`bt=`/`dec=`/`thread=`) |
| [`xclock`](xclock/README.md) | acelerar/frear/pausar o tempo simulado |
| [`xinfer`](xinfer/README.md) | inferência ONNX dentro do frame |
| [`xjoystick`](xjoystick/README.md) | controle do ownship por joystick físico |
| [`xlog`](xlog/README.md) | log com nível, sintaxe de stream e buffer em memória |
| [`xmsg`](xmsg/README.md) | mensagens/telemetria configuráveis por EDL |
| [`xplugin`](xplugin/README.md) | carga dinâmica de modelos em runtime (`dlopen`) |
| [`xpyembed`](xpyembed/README.md) | Python embarcado dentro do frame |
| [`xrandom`](xrandom/README.md) | derivação de sementes reprodutíveis |
| [`xrlbridge`](xrlbridge/README.md) | ponte de comando/observação para treino de RL |
| [`xtacview`](xtacview/README.md) | exportação da simulação para o Tacview |
| [`xtrack`](xtrack/README.md) | contato hostil mais próximo (radar nativo) |

**Por que seis são `shared_library()` e as outras não.** `xboard`, `xlog`, `xtrack`, `xrlbridge`,
`xinfer` e `xpyembed` cruzam a fronteira `dlopen` host↔plugin — host e modelo precisam enxergar a
**mesma** cópia de algum estado ou API em tempo de execução, e uma lib estática daria a cada lado a
sua própria cópia, silenciosamente. `xtacview`, `xclock`, `xjoystick`, `xmsg` e `xplugin` nunca
cruzam essa fronteira (nenhum modelo as inclui) e ficam `static_library()` — um plugin que linkasse
uma delas ganharia esse mesmo problema ao contrário. `xrandom` é a única header-only: função pura,
sem estado nenhum para sincronizar. Cada README explica o "por quê" do próprio caso; o `.claude/
rules/host-app-src.md` trava a regra geral, e `CLAUDE.md` tem o histórico completo.
