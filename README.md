# poc-mixr

Prova de conceito para desenvolver **novos modelos de simulação** sobre o framework
[MIXR](https://mixr.dev) (fork ASA, pacote Conan `mixr/1.0.5`) e sobre o
[BehaviorTree.CPP v3](https://github.com/BehaviorTree/BehaviorTree.CPP)
(`behaviortree.cpp.asa/3.5.6`).

O MIXR **não** é o objeto de desenvolvimento: entra como dependência binária, resolvida pelo
Conan. O que se desenvolve aqui são os modelos — players, dinâmicas, sensores, sistemas e a
camada de decisão (UBF + árvore de comportamento) — e a forma de encaixá-los na estrutura que o
framework já dirige.

O fork empacotado é **headless**: não publica `mixr_graphics`, `glut`, `instruments` nem
`ighost`. Não existe `GlutDisplay` aqui, e toda a visualização é feita por **Tacview Real-Time
Telemetry**, exportada pela biblioteca compartilhada [shared/xtacview/](shared/xtacview/).

---

## Pré-requisitos

- **Conan ≥ 2.0**, com os pacotes `mixr/1.0.5` e `behaviortree.cpp.asa/3.5.6` disponíveis no
  remote local (JSBSim, protobuf e OpenRTI vêm como dependências transitivas do MIXR)
- **Meson ≥ 1.0** e **Ninja**
- **GCC ≥ 7** ou **Clang ≥ 5** (o projeto compila em C++17)
- Opcional: **Tacview** (Advanced ou Standard) para receber a telemetria ao vivo

---

## Build

Toolchain: **Conan 2.x** → **Meson/Ninja** → **Makefile** (orquestra).

```bash
make configure   # conan install (Debug) + meson setup --reconfigure
make build       # compila TODOS os subprojetos
make install     # instala em dist/
make clean       # remove build/ e dist/
make help        # lista os alvos disponíveis
```

Cada subprojeto vira um executável independente em `build/src/NN-slug/src/<slug>`.

`dist/` **não é auto-contido**: os binários carregam no rpath o caminho das `.so` dentro do
cache do Conan, e leem `configs/`/`data/` por caminho relativo.

---

## Rodar

**Sempre a partir da raiz do repositório** — todos os binários resolvem `configs/` e `data/` por
caminho relativo.

```bash
make run-native-stack      # Tacview Real-Time Telemetry na porta 1234; Ctrl+C encerra
make check-native-stack    # verifica o determinismo (1, 2 e 4 threads de tempo crítico)

make run-tc-agent          # a poc/13 com agente de tempo crítico próprio (AgentTC)
make check-tc-agent        # mesmo determinismo, agora com a decisão dentro do frame
make compare-13-14         # lista o que difere entre os dois subprojetos
```

Ou direto pelo binário, também a partir da raiz:

```bash
./build/src/13-native-stack/src/native-stack
./build/src/14-tc-agent/src/tc-agent
```

Opções de linha de comando: `-f <arquivo>` (cenário alternativo), `-threads <N>` (força o número
de threads de tempo crítico) e `-deterministic <N>` (roda N frames de passo fixo e despeja o
estado, em vez de rodar em tempo real).

---

## Estrutura

```
poc-mixr/
├── src/NN-slug/       subprojetos: um executável independente por pasta, todos compilados juntos
├── shared/xtacview/   exportação para o Tacview (OutputHandler nativo do recorder)
├── contexts/          material de consulta sobre MIXR e BehaviorTree.CPP (ver abaixo)
├── conanfile.py       dependências binárias
├── meson.build        raiz: resolve as libs por pkg-config e dá subdir() em cada subprojeto
└── Makefile           orquestra Conan + Meson
```

### Anatomia de um subprojeto

```
src/NN-slug/
├── meson.build            só faz subdir('./src')
├── configs/               cenário EDL (.epp) + árvores de comportamento (.xml)
├── data/                  dados vendorizados (jsbsim/, terrain/, recordings/)
├── include/ e src/
│   ├── domain/            regras de negócio puras — sem MIXR, sem BT — testáveis isoladas
│   ├── ubf/ | bt/         adaptadores: percepção/decisão/atuação, nós da árvore
│   ├── x<nome>/           classes MIXR próprias (namespace mixr::x<nome>) + factory própria
│   ├── mixr_factory.*     encadeia a factory própria antes das do framework
│   └── main.cpp           fino: constrói a Station e roda o laço; não implementa comportamento
```

A divisão vale a regra: **"o que fazer" mora em `domain/`; "como conectar" mora nas
factories e adaptadores; `main.cpp` só orquestra.**

O par estrutura/comportamento é o que o MIXR propõe: a **estrutura** do cenário (quais players
existem, com quais subsistemas, em que taxa, com quantas threads) é declarada em **EDL**
(`configs/*.epp`) e lida em tempo de carga; o **comportamento** é o C++. Reconfigurar o cenário
não recompila nada.

### `shared/xtacview/`

Única exportação Tacview do repositório — nenhum `main.cpp` monta stream ACMI. `TacviewOutput` é
um `recorder::OutputHandler` de verdade, declarado na cadeia nativa do slot `dataRecorder` da
`Station`; o framework empurra cada registro do gravador para ele, que traduz em linhas ACMI e
transmite (porta 1234) e/ou grava o `.acmi`.

As armadilhas confirmadas rodando estão documentadas em
[shared/xtacview/TacviewOutput.cpp](shared/xtacview/TacviewOutput.cpp) — a primeira delas
economiza horas: **`dataLogTime` é slot do `Player`, nasce zero, e sem
`dataLogTime: ( Seconds 0.1 )` o player simplesmente nunca aparece no Tacview.**

### `contexts/`

Duas camadas de consulta sobre as libs:

| caminho | conteúdo |
|---|---|
| `contexts/MIXR-CONTEXT.md` | como o MIXR funciona por dentro: classes, macros, ciclo de vida, EDL, recorder |
| `contexts/MIXR-PATTERN-CONTEXT.md` | como se escreve uma aplicação MIXR; a §0 lista o que o fork empacotado **não** tem |
| `contexts/BTCPP-CONTEXT.md` | BehaviorTree.CPP **3.5.6** — nada ali vale para a v4 |
| `contexts/src/mixr/` | **fonte completo** do fork ASA 1.0.5: `src/` com as implementações, `include/`, `deps/`, `doc/` |
| `contexts/src/BehaviorTree.CPP/` | fonte da 3.5.6, com `examples/`, `sample_nodes/` e `tests/` |

Os `.md` são destilação; `contexts/src/` é a verdade. Quando o comportamento observado
contraria o esperado, a resposta está no `.cpp` do framework — não no header.

`contexts/src/mixr/` é **git-ignored** (é uma cópia local, não vem num clone limpo). Sem ela,
os headers instalados pelo Conan (`~/.conan2/p/b/mixr*/p/include/mixr/...`) são o fallback — e,
em caso de divergência, quem vale é o pacote, que é o que está linkado.

---

## Onde ler mais

- **[CLAUDE.md](CLAUDE.md)** — guia operacional do repositório: comandos, arquitetura em uma
  tela, gotchas de rpath, de unidades e do recorder.
- **[src/13-native-stack/README.md](src/13-native-stack/README.md)** — anatomia completa de um
  subprojeto, do `.epp` ao frame de tempo crítico: como o framework dirige o código da
  aplicação, o que dá para herdar, o que se paga por isso e as armadilhas medidas rodando.
- **[src/14-tc-agent/README.md](src/14-tc-agent/README.md)** — a poc/13 com uma única troca (o
  agente do UBF passa a decidir na fase 3 do frame): onde uma decisão deve rodar, o que isso faz
  com o determinismo e com a taxa de decisão, medido nos dois binários.
