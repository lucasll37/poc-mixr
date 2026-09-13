# `my-event` — exercício: um evento próprio do MIXR, emitido e tratado pela mesma classe

## O que esta poc isola

Nenhuma decisão (sem BT, sem UBF, sem agente) — só o **mecanismo de evento** do MIXR
(`mixr::base::Component::event()`), de ponta a ponta:

- **O modelo**: [`models/players/Beacon/`](../../../models/players/Beacon/) — um
  `mixr::models::Player` mínimo (sem `dynamicsModel`, sem `pilot`, sem sensor) que, periodicamente,
  transmite um `PingMessage` (payload não-nulo: remetente, sequência, mensagem) para os demais
  `Beacon`s do cenário — e **trata** o mesmo evento quando é ele quem recebe. A mesma classe nos
  dois papéis, o primeiro caso deste repositório em que isso acontece.
- **O evento**: `events::EID_PING`/`events::PingMessage` —
  [`models/events/payloads/EID_PING/`](../../../models/events/payloads/EID_PING/), novo nesta
  mudança. Ver [`models/events/README.md`](../../../models/events/README.md) para a convenção
  completa (o que é um evento aqui, as duas formas de despacho, por que o payload mora numa
  `shared_library()` de fronteira de plugin).

## O cenário

Três `Beacon` parados (`beacon1`/`beacon2`/`beacon3`), formando um triângulo de ~2 NM de lado —
só para aparecerem separados no Tacview, já que o alcance do ping não é filtrado por distância
nenhuma (mesmo caminho (b) de `xnative::AlertDatalink::broadcastAlert()`, `models/players/A-4`: o
broadcast alcança todo player local ativo). Cada um com um `pingInterval` diferente (3s/4s/6s) e
uma `pingMessage` própria — de propósito, para que a leitura do log mostre os três relógios saindo
de fase (todos emitem juntos no primeiro frame, porque `pingTimer` nasce zerado, e depois cada um
segue seu próprio ritmo).

## Rodar

**Ver o log de verdade (o motivo de existir esta poc) — use `./dist/bin/node`, não `./app`:**

```bash
./dist/bin/node src/poc/my-event/configs/scenario.edl
```

`node` é o runner headless (`src/node/README.md`) — sem TUI, só as linhas `LOG(...)` no console,
exatamente o que mostra o ping sendo emitido e tratado:

```
[INFO] [Beacon] beacon1 emitiu ping #1 ("temperatura 21C, sem anomalias") para 2 player(es)
[INFO] [Beacon] beacon2 recebeu ping #1 de beacon1: "temperatura 21C, sem anomalias"
[INFO] [Beacon] beacon3 recebeu ping #1 de beacon1: "temperatura 21C, sem anomalias"
```

`Ctrl+C` encerra de forma limpa. `./app -folder src/poc -scenario my-event` também roda (a TUI
mostra os três `Beacon` na aba Players, posição no Mapa, contadores de instância na aba Memória) —
mas o `./app` desliga o log para não sujar o desenho da TUI (`xlog::setConsoleEnabled(false)`), e
os campos `sent=`/`recv=`/`bt=`/`dec=` do dump `-deterministic` são específicos de
`AlertDatalink`/UBF (`models/players/A-4`) — `Beacon` não os preenche, então saem `--`/`0`, o que é
esperado e não indica falha: a prova de que os pings estão acontecendo é o contador
`meta=PingMessage ... tc=N` (N cresce com o número de pings emitidos) e, principalmente, o log.

```bash
./dist/bin/app -folder src/poc -scenario my-event -deterministic 200   # sem log, so' o dump
python3 src/ui/scripts/edl_lint.py src/poc/my-event/configs/scenario.edl   # lint leve (best-effort)
./dist/bin/edlcheck src/poc/my-event/configs/scenario.edl                  # o parser de verdade
```

## O que foi medido rodando

- `edlcheck` aceita o cenário e confirma que `libBeacon.so` responde exatamente por `Beacon` e
  `PingMessage` (o `provides:` declarado).
- Em `-deterministic 200` (4s simulados): os três `Beacon` aparecem no dump com posição correta e
  `bt=--`/`sent=0`/`recv=0` (campos que este modelo não preenche — ver acima), e
  `meta=PingMessage count=0 mc=1 tc=6` — zero instâncias vivas ao final (sem vazamento: todo
  `PingMessage` é `unref()`'d logo após a entrega), no máximo uma viva por vez (a entrega é
  síncrona), e several construídas ao longo da execução.
- Em tempo real (`./dist/bin/node`, ~14s): os três `Beacon` trocam ping nos intervalos
  configurados (3s/4s/6s), cada um recebendo de AMBOS os outros dois, com o texto/sequência/
  remetente corretos — confirmado lendo o log linha a linha.
