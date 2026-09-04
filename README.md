# poc-mixr

Metaprojeto de exploração do framework [MIXR](https://mixr.dev) (pacote Conan `mixr/1.0.5`) e do
[BehaviorTree.CPP v3](https://github.com/BehaviorTree/BehaviorTree.CPP) (`behaviortree.cpp.asa/3.5.6`).
Não é uma aplicação de simulação em si — é um conjunto de sugestões testadas de **como usar** o
MIXR: o que o framework já resolve pronto, o que sobra para escrever, e qual o preço de cada
escolha de integração.

O cenário usado em todos os subprojetos é o mesmo: aeronaves de patrulha voando com física de voo
realista, cada uma controlada por uma **lógica de decisão** própria — um comportamento autônomo
que escolhe rumo, altitude e reação a eventos, como detectar uma aeronave intrusa. O MIXR entra
como motor de simulação (física, sensores, comunicação, gravação de dados); o que se desenvolve
aqui é essa lógica de decisão e as diferentes formas de conectá-la ao motor.

O MIXR **nunca é modificado** — entra como dependência binária, resolvida pelo Conan. Ele fornece
as entidades simuladas (aqui chamadas de **players** — aeronaves, mísseis etc.) e a infraestrutura
de execução; os **modelos** deste repositório (players concretos, dinâmica de voo, sensores e a
lógica de decisão) são carregados pelo executável em tempo de execução como plugins (`dlopen`).
Cada subprojeto sob `src/poc/` isola **uma** variável de integração — onde a decisão roda, como um
player chega à simulação, em que linguagem a lógica de decisão é escrita — mantendo o resto do
cenário idêntico ao das outras, para comparar custo e comportamento sem ruído de diferenças
acessórias.

O fork empacotado é **headless** (sem `mixr_graphics`/`glut`/`instruments`/`ighost`): toda
visualização é feita via **Tacview Real-Time Telemetry**.

> Documentação, comentários de código e mensagens de console são em português do Brasil.
> Identificadores, nomes de slot e nomes de fábrica ficam em inglês — são os originais do MIXR.

---

## Índice

1. [Funcionalidades exploradas](#1-funcionalidades-exploradas)
2. [Pré-requisitos](#2-pré-requisitos)
3. [Build](#3-build)
4. [Rodar](#4-rodar)
5. [Verificar](#5-verificar)
6. [Como o projeto se organiza](#6-como-o-projeto-se-organiza)
7. [Subprojetos](#7-subprojetos)
8. [Os modelos são plugins](#8-os-modelos-são-plugins)
9. [`contexts/` — onde consultar o framework](#9-contexts--onde-consultar-o-framework)

---

## 1. Funcionalidades exploradas

| # | funcionalidade | onde |
|---|---|---|
| 1 | Carga dinâmica de modelos — o modelo é um `.so` aberto em tempo de execução, não compilado junto do executável; o mesmo binário roda modelos diferentes sem recompilar | `models/` |
| 2 | Elevação de terreno real (dados SRTM)| `shared/data/terrain/srtm/` |
| 3 | Exportação da simulação para o Tacview (visualizador de voo em tempo real), reaproveitando o próprio mecanismo de gravação de dados do MIXR | `shared/xtacview/` |
| 4 | Física de voo completa (6 graus de liberdade) via JSBSim, o simulador de voo já integrado ao MIXR | `models/flight/` |
| 5 | Decisão em paralelo sem perder previsibilidade — várias aeronaves decidindo ao mesmo tempo, em threads diferentes, chegam ao mesmo resultado rodando com 1, 2 ou 4 threads | `src/poc/multi-thread/` |
| 6 | Lógica de decisão como uma árvore de comportamento (BehaviorTree.CPP) carregada de um arquivo XML — trocar a política não recompila nada | `models/flight/src/bt/` |
| 7 | Controle por joystick físico, com o piloto automático assumindo sozinho quando não há joystick conectado | `shared/xjoystick/` |
| 8 | Interoperabilidade entre processos via DIS (protocolo padrão de simulação distribuída): uma aeronave roda num processo separado e é vista pelas outras só pela rede | `src/poc/bandit-dis/` |
| 9 | Log e telemetria configuráveis pelo próprio arquivo de cenário, sem recompilar nada | `shared/xlog/`, `shared/xmsg/` |
| 10 | Detecção de vazamento de memória usando os contadores de instância que o próprio MIXR já mantém internamente | `shared/xboard/` |
| 11 | Painel de controle interativo em terminal — pausar/acelerar o tempo, navegar num mapa, pausar a simulação quando a decisão de uma aeronave atingir um ponto específico da árvore | `app/` |
| 12 | Lógica de decisão substituída por uma rede neural treinada previamente (`.onnx`), executada a cada ciclo de simulação, sem depender de Python em tempo de execução | `src/poc/onnx-policy/` |
| 13 | Lógica de decisão escrita em Python, editável em arquivo `.py` e recarregada sem recompilar o projeto | `src/poc/python-flight/` |

---

## 2. Pré-requisitos

| ferramenta | versão | por quê |
|---|---|---|
| **Conan** | ≥ 2.0 | resolve `mixr/1.0.5`, `behaviortree.cpp.asa/3.5.6`, `ftxui/7.0.3` |
| **Meson** | ≥ 1.0 | sistema de build |
| **Ninja** | qualquer | *backend* do Meson |
| **GCC ≥ 7** ou **Clang ≥ 5** | — | o projeto compila em **C++17** |
| **gzip** | qualquer | descomprime os tiles SRTM na primeira execução |
| **Tacview** (Advanced ou Standard) | opcional | recebe a telemetria ao vivo |

Outras bibliotecas vêm como dependências **transitivas** do MIXR — não
precisam ser pedidas à parte.

---

## 3. Build

Toolchain: **Conan 2.x** → **Meson/Ninja** → **Makefile** (orquestra). São **três projetos
Meson** em ordem obrigatória — o(s) modelo(s) são construídos **antes** do host, porque o host só
consome o `.so` deles:

```bash
make configure   # conan install + meson setup do HOST -> build/
make build       # o host (depende so do sdk) -> build/
make install     # sincroniza os plugins e instala o host em dist/
make test        # as suítes do host e do(s) modelo(s) -- precisa de -Dtests=true no configure
make clean       # remove todos os build*/ e o dist/
make help        # lista todos os alvos, com descrição
```

No dia a dia basta `make configure && make build && make install`.

**AddressSanitizer:** `make test-asan` instrumenta host e modelo, roda sob LeakSanitizer e
reverte no final.

---

## 4. Rodar

**Sempre a partir da raiz do repositório** — os binários resolvem `configs/`/`data/` por caminho
relativo. `single-thread`/`multi-thread` são alternativas entre si; qualquer um roda sozinho ou ao
lado do `bandit-dis`, que entrega a aeronave intrusa só pela rede, e o Tacview conecta na porta
indicada (*File > Real-Time Telemetry*).

| subprojeto | comando | Tacview | decisão |
|---|---|---|---|
| `app` | `make run-app` | 1236 | painel interativo — **comece por aqui** |
| `single-thread` | `make run-single-thread` | 1234 | numa única thread, em segundo plano |
| `multi-thread` | `make run-multi-thread` | 1234 | em paralelo, uma thread por aeronave |
| `bandit-dis` | `make run-bandit-dis` | 1235 | nenhuma — só uma aeronave, por joystick ou piloto automático, emitida via rede (DIS) |
| `python-flight` | `make run-python-flight` | 1237 | em Python, editável sem recompilar |
| `onnx-policy` | `make run-onnx-policy` | 1238 | por uma rede neural (`.onnx`) |

---

## 5. Verificar

```bash
meson configure build -Dtests=true   # a suíte fica atrás desta opção
make test                            # as suítes do host e do(s) modelo(s)

make check-single-thread   # determinismo com a decisão no laço de background
make check-multi-thread    # determinismo com os agentes decidindo em paralelo, na fase 3
make check-python-flight   # o mesmo, decidindo em Python (um GIL para os quatro)
make check-onnx-policy     # o mesmo, inferindo uma rede neural (uma sessão ONNX)
make compare-single-multi  # lista o que difere entre os dois subprojetos (deve ser só o agente)
make test-asan             # LeakSanitizer na single-thread (build separado, lento)
```

---

## 6. Como o projeto se organiza

```
poc-mixr/
├── app/                    painel de controle (TUI) -- unico ocupante da pasta, fora de src/
├── src/
│   ├── poc/                as POCs propriamente ditas: cada pasta isola uma variavel de
│   │   │                   integracao, mantendo o resto do cenario identico as demais
│   │   ├── single-thread/  decide uma aeronave de cada vez, numa unica thread, em segundo plano
│   │   ├── multi-thread/   decide todas ao mesmo tempo, cada uma na sua propria thread
│   │   ├── bandit-dis/     so uma aeronave, sem logica de decisao, emitida via rede (DIS)
│   │   ├── python-flight/  a multi-thread com a logica de decisao em Python
│   │   └── onnx-policy/    a multi-thread com a logica de decisao numa rede neural
│   ├── rl/                 aplicacao de verdade, nao poc: treino de aprendizado por reforco
│   │                       (RL) contra a mesma simulacao
│   └── node/               aplicacao de verdade, nao poc: placeholder do no de producao que
│                           atendera solicitacoes de execucao
├── models/                 o(s) MODELO(s) -- projetos Meson a parte, carregados como plugin (§8)
│   ├── flight/             modelo de producao (carregado pelos subprojetos de voo)
│   ├── missile/            segundo modelo -- demo academica de missil guiado 6-DOF
│   ├── fixtures/stub/      modelo minimo, so contra o SDK -- prova que o contrato basta
│   └── plugins/            deposito flat dos .so compilados (e onde um .so de terceiro
│                           entraria) -- make install sincroniza daqui para dist/
├── shared/                 bibliotecas x<nome> reaproveitadas entre subprojetos
├── tests/                  suite do HOST (a de cada modelo vive dentro do proprio models/<nome>/)
├── contexts/               material de consulta sobre MIXR e BehaviorTree.CPP (§9)
├── conanfile.py            dependencias binarias (mixr, BehaviorTree.CPP, ftxui, gtest)
├── meson.build             raiz: resolve as libs por pkg-config e da subdir() em cada subprojeto
└── Makefile                orquestra Conan + Meson
```

Três regras estruturam todo subprojeto e todo modelo:

1. **"O que fazer" mora em `domain/`; "como conectar" mora nas factories e adaptadores;
   `main.cpp` só orquestra.** `domain/` não inclui um header sequer do MIXR ou do
   BehaviorTree.CPP — dá para testar a política sem levantar uma simulação.
2. **Um arquivo, uma questão.** Nenhum `main.cpp` de centenas de linhas — cada etapa é um módulo
   próprio, cada header abrindo com o *porquê* daquele passo.
3. **Estrutura vem do EDL, comportamento vem do C++.** Players, subsistemas, taxas, threads —
   tudo isso é `configs/*.epp`, lido em tempo de carga. Reconfigurar não recompila nada.

---

## 7. Subprojetos

`single-thread` e `multi-thread` são o **mesmo modelo** — mesmo cenário, mesmas aeronaves, mesmo
comportamento — trocando só **onde a decisão roda**: a primeira decide uma aeronave de cada vez,
numa única thread, em segundo plano; a segunda decide todas ao mesmo tempo, cada uma na sua
própria thread. Os nomes falam só dessa diferença: as duas rodam sobre a mesma infraestrutura de
paralelismo do MIXR e passam pela mesma verificação de determinismo.

`python-flight` e `onnx-policy` partem dessa mesma base (a `multi-thread`) e trocam **a
tecnologia da lógica de decisão**: a primeira por scripts Python, a segunda por uma rede neural —
nos dois casos, sem perder o determinismo mesmo decidindo em paralelo.

`bandit-dis` é de outra natureza: não tem lógica de decisão nenhuma, só uma aeronave pilotada por
joystick (ou por um piloto automático, quando não há joystick conectado), emitida via rede —
existe para provar que dois processos distintos podem trocar uma aeronave só pela rede, como se
fossem simuladores de fabricantes diferentes.

`app` reúne as pilhas acima num painel de controle interativo — controle de tempo, mapa navegável
e inspeção ao vivo do estado da simulação — em vez de uma linha de status em texto.

---

## 8. Os modelos são plugins

`domain/`, `bt/`, `ubf/` e o resto da lógica de decisão **não** moram dentro de `src/poc/<poc>/`.
Cada modelo é um **projeto Meson independente**, em `models/`, construído numa etapa anterior
(`make models`) e carregado pelo host via `dlopen`, através de um contrato de interface binária
(ABI) publicado (`shared/xplugin/`):

```bash
make sdk      # publica o contrato (PluginAbi.hpp) + as libs de fronteira -> dist/
make models   # constroi CADA modelo a parte -> deposita em models/plugins/*.so
make build    # o host (depende so do sdk) -> build/ -- nao precisa dos modelos pra compilar
make install  # sincroniza models/plugins/ -> dist/lib/mixr-plugins/ e instala o host -> dist/
```

`models/plugins/` é um depósito comum: um `.so` compilado por este repositório e um `.so` de
terceiro (compilado em outro lugar, só copiado para lá) são indistinguíveis a partir desse ponto —
os dois viram visíveis ao host somente quando `make install` sincroniza para `dist/`.

---

## 9. `contexts/` — onde consultar o framework

Duas camadas: os `.md` destilados (leitura rápida) e o **código-fonte completo das libs** (a
verdade) — `contexts/src/mixr/` e `contexts/src/BehaviorTree.CPP/`.

| caminho | conteúdo |
|---|---|
| `contexts/MIXR-CONTEXT.md` | como o MIXR funciona por dentro |
| `contexts/MIXR-PATTERN-CONTEXT.md` | Boas praticas de desenvolvimento no MIXR |
| `contexts/BTCPP-CONTEXT.md` | BehaviorTree.CPP **3.5.6** — nada ali vale para a v4 |