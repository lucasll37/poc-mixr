# `Beacon` — modelo de exercício: emitir e tratar um evento próprio no MIXR

## O que é isto

`Beacon` é um modelo (plugin `.so`, carregado por `dlopen` como qualquer outro sob `models/`) que
**não decide nada** — não tem árvore de comportamento, não tem UBF, não voa. Ele existe só para
mostrar, de ponta a ponta, como se implementa um **evento próprio** no MIXR (ver
[`../../events/README.md`](../../events/README.md) para a convenção completa): uma constante
(`events::EID_PING`), um payload não-nulo (`events::PingMessage`, com campos de verdade — não os
dois `uint32` crus de um `REID_MARKER` do recorder), e uma classe que **emite** o evento **e**
**trata** o mesmo evento — os dois papéis na mesma classe, ao mesmo tempo.

`mixr::models::xBeacon::Beacon` herda `mixr::models::Player` **direto** — o mesmo padrão mínimo de
`mixr::models::Building` (só `getMajorType()` sobrescrito) — sem `dynamicsModel`, sem `pilot`, sem
sensor: o player fica parado onde o `.edl` posicionar. A cada `pingInterval` segundos (default 5s),
cada `Beacon` do cenário transmite um `PingMessage` (remetente, sequência, mensagem) para os demais
`Beacon`s locais ativos — e todo `Beacon`, inclusive o próprio emissor de outros pings, trata o
evento recebido (conta, loga, guarda o último remetente/mensagem).

Cenário de demonstração: [`../../../src/poc/my-event/`](../../../src/poc/my-event/).

## Por que não tem `domain/`, `bt/`, `ubf/`

O propósito deste modelo é o **mecanismo de evento**, não decisão em camadas — não há regra de
negócio pura para isolar em `domain/`, não há árvore de comportamento, não há percepção/decisão/
ação separadas:

```
Beacon/
├── include/
│   ├── Beacon.hpp             # a classe: slots, getters, e as declaracoes de reset()/updateData()/event()
│   └── xnative/factory.hpp    # registro das duas classes deste plugin
├── src/
│   ├── Beacon.cpp              # a implementacao -- emitir (broadcastPing) e tratar (onPingEvent)
│   ├── xnative/factory.cpp     # a fabrica (Beacon + o payload PingMessage) -- MESMO desenho de A-4
│   └── plugin.cpp              # a fronteira C: so' MIXR_PLUGIN_DEFINE, ver o cabecalho dele
├── tests/
│   ├── native/test_beacon.cpp   # a factory + dois Beacon de bancada (WorldModel, sem Station) trocando ping
│   └── check_contract.sh        # forma do .so: 1 simbolo T, deps resolvidas
├── docs/ARCHITECTURE.md  # o "porque" das decisoes deste modelo especifico
├── CHANGELOG.md
├── Makefile              # build autocontido -- ver abaixo
└── meson.build           # UM artefato: libBeacon.so (sem BehaviorTree.CPP -- sem arvore nenhuma)
```

A separação `include/Beacon.hpp`+`src/Beacon.cpp` (a classe) de `xnative/factory.{hpp,cpp}` (o
registro) e `src/plugin.cpp` (a fronteira C) é a MESMA de `models/players/air/A-4`, só que com duas
classes em vez de dezenas — o desenho mínimo continua sendo o de sempre, não o atalho "tudo num
arquivo só" (`models/template/src/mirror.cpp`), que existe para um propósito diferente (mirror de
contrato, nunca produção de verdade — ver o aviso no topo do próprio arquivo).

`PingMessage` (o payload) **não** mora aqui — mora em
[`../../events/payloads/EID_PING/`](../../events/payloads/EID_PING/), na `shared_library()`
`libevents.so` que qualquer modelo pode linkar via o SDK (`poc-mixr-sdk.pc`), pelo mesmo motivo já
documentado no cabeçalho de `PingMessage.hpp`: um `dynamic_cast` de um payload recebido de OUTRO
`.so` só é seguro se a classe do payload vier de uma lib linkada por ambos os lados.

## Slots

| slot | tipo | default | efeito |
|---|---|---|---|
| `pingInterval` | `Time` | `5.0 s` | intervalo entre um ping e o próximo |
| `pingMessage` | `String` | `"ping"` | texto levado no payload de cada ping |

`Beacon` **não filtra alcance nem lado** — mesmo caminho (b) de
`xnative::AlertDatalink::broadcastAlert()` (`models/players/air/A-4`): todo `Beacon` local ativo,
exceto o próprio emissor, recebe todo ping.

## Compilar e testar, sozinho

Este diretório é um projeto de build **independente** — não é preciso abrir o resto do repositório
para compilar só isto. O único pré-requisito é que o projeto principal já tenha publicado o SDK de
plugin, uma vez:

```bash
cd ../../.. && make configure && make sdk

cd models/others/Beacon
make build              # -> ./dist/lib/mixr-plugins/libBeacon.so (bare `make` so mostra `make help`)
make test               # native (Beacon de verdade, sem Station) + forma do .so
make check-organization # opcional -- linter de organizacao interna
make install-core       # copia o .so para ../../../plugins/ -- ver a proxima secao
```

`make help` lista todos os alvos. `./build` e `./dist` nascem e ficam dentro **deste**
diretório — nada aqui escreve fora dele, exceto `make install-core`.

## Por que existe um passo separado para "publicar no core"

Um cenário só encontra um `.so` de modelo dentro de uma pasta específica, relativa à raiz do
repositório inteiro (não à raiz deste diretório). `make install-core` copia para
`../../../plugins/` (o depósito compartilhado com terceiro); quem sincroniza dali para `dist/`,
onde um cenário de fato procura, é o `make install` do projeto raiz.

Um `.edl` referencia este modelo assim:

```
( PluginModule  file: "libBeacon.so"
   provides: { Beacon PingMessage } )
```

(os dois nomes têm que bater **exatamente** com o que `libBeacon.so` exporta — `PingMessage` entra
porque o payload é registrado no próprio `factoryNames()` deste plugin, não porque o `.edl`
construa um por nome; ver o cabeçalho de `src/plugin.cpp`.)

## Se você quiser mais contexto

- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — o "porquê" de cada decisão deste modelo
- [`../../events/README.md`](../../events/README.md) — a convenção de evento que este modelo
  exercita (o que é um evento aqui, as duas formas de despacho, por que o payload mora numa
  `shared_library()`)
- [`../../template/docs/CONTRATO.md`](../../template/docs/CONTRATO.md) — a lista completa e
  autoritativa do que QUALQUER modelo precisa fazer para o core carregá-lo e rodar com ele
- [`../../../CLAUDE.md`](../../../CLAUDE.md), seção "O MODELO é um plugin, construído numa etapa
  PRÉVIA" — visão geral de `models/`, o contrato de plugin, e o fluxo de build orquestrado pelo
  Makefile da raiz
