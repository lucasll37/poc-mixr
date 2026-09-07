# `libs/xmsg` — mensagens configuráveis por EDL

Escolher **o que** sai da simulação e **quando** sai vira configuração no `.edl`, não recompilação:
um `( MsgFeed )` amostra os players, avalia condições (mudou / cruzou limiar / muda rápido demais)
e entrega o resultado a uma lista de destinos, em NDJSON.

## Como se usar

Um `( MsgFeed )` no `components:` da `Station`, no mesmo slot genérico onde os agentes já moram
(exemplo real, de `src/poc/dis/single-thread/configs/scenario.edl.in`):

```
msgFeed: ( MsgFeed

   trackManager: twsTrkMgr        // nome do TrackManager (grupo TRACK)
   maxPlayers:   64
   healthEvery:  ( Seconds 10 )

   sinks: {
      ( MsgFileSink
         fileName: "./src/poc/dis/single-thread/data/messages/mission.jsonl"
         flushEvery: ( Seconds 2 ) )
   }

   messages: {

      // "posicao/velocidade/altitude a cada instante". Sem 'when:' sai
      // todo ciclo, limitada por 'every:'. players vazio = todos,
      // inclusive o bandit1 que so existe na rede.
      ( MsgReport name: telemetria  players: { }
         labels: { player side mode }
         fields: { latDeg lonDeg altMslM altAglM hdgDeg speedKts
                   machNum climbMps fuelFrac }
         every: ( Seconds 1.0 ) )

      // "mudanca de altitude": deadband medido contra o ultimo valor
      // EMITIDO -- uma subida lenta da um evento por degrau de 100 m,
      // nao um por ciclo.
      ( MsgReport name: mudanca-altitude
         players: { falcon1 falcon2 falcon3 falcon4 }
         labels: { player }
         fields: { altMslM altAglM climbMps hdgDeg }
         when: { ( MsgChanged field: altMslM by: ( Meters 100 ) ) } )

      // Filtro composto: baixo E descendo rapido.
      ( MsgReport name: baixo-e-descendo
         players: { falcon1 falcon2 falcon3 falcon4 }
         labels: { player }
         fields: { altAglM terrainElevM climbMps hdgDeg speedKts }
         match: all
         when: {
            ( MsgThreshold field: altAglM
                 below: ( Meters 500 )  clear: ( Meters 700 ) )
            ( MsgRate field: altMslM  below: -10.0
                 window: ( Seconds 1.0 ) )
         }
         every: ( Seconds 5.0 ) )
   }
)
```

(o `.edl.in` real ainda declara uma quarta mensagem em `messages:` — `falha-motor`, outro
`MsgThreshold`, sobre `engThrustAsymFrac` com `hold:` — omitida aqui por brevidade; toda linha
mostrada acima é copiada tal qual do arquivo.)

Nada disso exige C++ novo do lado de quem consome — `messages/mission.jsonl` recebe uma linha
NDJSON por emissão, mais uma mensagem `msgHealth` a cada `healthEvery:` reportando quanto foi
suprimido. Ver `libs/xmsg/FieldCatalog.cpp` para a lista fechada de nomes aceitos em `fields:`/
`field:` (nome desconhecido é `LOG(ERROR)` e desativa o `MsgFeed` inteiro naquele `reset()` —
nunca ignorado em silêncio, mas o resto do cenário continua carregando normalmente).

**O diretório do sink tem de existir antes de rodar** — mesma causa muda que o `TacviewOutput` já
tem sem `data/recordings/`: `MsgFileSink::open()` não cria diretório, só abre o `ofstream`; sem
`data/messages/` no disco a mensagem de erro (`"...para gravacao -- o diretorio existe?"`) pelo
menos não é silenciosa, mas o cenário perde o sink. Cada poc que declara `msgFeed:` tem um
`data/messages/.gitkeep`.

## Por que não é o `mixr::recorder`

A resposta óbvia — usar `DataRecorder`/`OutputHandler`/`recordData()`, que já existe no MIXR — foi
avaliada e descartada antes de escrever qualquer coisa. O schema `DataRecord.proto` é fechado:
`PlayerState` carrega só `pos`/`angles`/`vel` (ECEF) e `damage` — **não há combustível, motor,
Mach nem AGL**, justamente as grandezas que `fields:` pede acima. Tokens REID de usuário
(1000-9999) são descartados em silêncio, e não existe primitiva nenhuma de mudança/limiar/
histerese no framework (grep por `hysteresis|Schmitt|Threshold|Debounce` em `include/mixr/`: zero
— ver o cabeçalho de `rules/Schmitt.hpp`). Remendar o `.proto` vendorizado está fora de cogitação —
o MIXR é dependência binária, não objeto de desenvolvimento neste repositório. O que se reaproveita
do recorder nativo é só a **forma**: EDL declarativo, cadeia de destinos com filtro por assinante
(`MsgSink::accepts()`), trabalho fora do frame de tempo crítico. O `Player` é lido direto
(`SnapshotSource.cpp`), do mesmo jeito que `ubf::FlightState::updateState()` e
`TacviewOutput::updateRadarScan()` já fazem.

## `rules/` é livre de MIXR

`Schmitt`, `Deadband`, `RateWindow` e `EmitGate` (`libs/xmsg/rules/`) não incluem nada do
framework — é a mesma separação que motivou tirar `domain::WorldView` de dentro de uma classe MIXR
(ver `CLAUDE.md`, seção "Testes automatizados"). Cada uma resolve **uma** questão:

| classe | questão | usada por |
|---|---|---|
| `Schmitt` | limiar com histerese (`clear`) + tempo mínimo de permanência (`hold`) | `MsgThreshold`, `MsgRate` |
| `Deadband` | "mudou mais que X desde a última vez que emiti" | `MsgChanged` |
| `RateWindow` | derivada sobre janela de tempo simulado (buffer fixo, `CAPACITY=256`) | `MsgRate` |
| `EmitGate` | intervalo mínimo entre emissões (`every:`) — **adia**, nunca descarta | `MsgReport` |

Testadas sozinhas, sem levantar `Station`, em `tests/domain/test_xmsg_rules.cpp` — é onde
comportamento de borda (repetir no platô, oscilar na fronteira, disparar no transiente, perder o
evento sob saturação) é travado.

## Armadilhas confirmadas

1. **Lista `{ a b }` no EDL põe o nome no OBJETO, não no slot.** `fields: { latDeg lonDeg }`
   chega como itens **anônimos** — o parser numera os slots (`"1"`, `"2"`, ...) e o nome vai no
   valor, como `base::Identifier` (não `base::String` — um `dynamic_cast<String>` sozinho falha
   em silêncio). Já `{ chave: valor }` põe o nome no slot. `MsgReport.cpp::readNames()` lê o
   objeto primeiro e só cai para `pair->slot()` depois, cobrindo as duas formas.
2. **Acumulador de tempo simulado precisa de tolerância.** `0.1` somado 10 vezes dá
   `0.9999999999999999` (abaixo de 1.0); `0.02` somado 50 vezes dá `1.0000000000000004` (acima) —
   e o erro **muda de sinal** entre o laço de 10 Hz (tempo real) e o de 50 Hz (`-deterministic`).
   Sem tolerância, um `hold:`/`every:`/`flushEvery: ( Seconds 1.0 )` armaria num passo num modo e
   noutro passo no outro. `rules/timeTolerance.hpp::reached()` (tolerância `1e-9`) é o que todo
   acumulador deste subsistema usa para comparar.
3. **Grupo inválido vira `null` no JSON, nunca `0.0`.** Um player recebido por DIS é clonado de um
   `template:` sem `dynamicsModel`, então toda grandeza de motor dele lê zero — e zero é um valor
   plausível de empuxo. `Snapshot::groupValid[]` (por `Group`: `Ident`, `Kinematics`, `AirData`,
   `Fuel`, `Engine`, `Track`, `Status`) separa "é zero" de "não existe"; uma condição sobre campo
   inválido não avalia, não gera borda e congela o nível — sem essa regra, `falha-motor` com
   `players: { }` acusaria falha permanente no intruso.
4. **`'clear'` é obrigatório em `MsgThreshold`, opcional em `MsgRate`.** Sem histerese, um valor
   tremendo em cima do limiar produz uma mensagem por ciclo, e o sintoma (enxurrada) não aponta
   para a causa — deixar isso como default seria empurrar a armadilha para quem configura o
   cenário. Em `MsgRate` a derivada já é um sinal suavizado e a detecção de borda sozinha já basta
   na maioria dos casos; quem quiser histerese ainda pode pedir `clear:`.
5. **`'hold:'` existe por causa do transiente de partida.** `JSBSimModel::reset()` não roda
   `FGTrim` — a aeronave começa destrimada, com excursões nos primeiros segundos que não são
   evento nenhum. `Schmitt` só arma depois de `hold` segundos contínuos do lado ligado.
6. **`MsgFileSink` não reusa o `recorder::PrintHandler`** (ao contrário do `libs/xlog`):
   `PrintHandler::printToOutput()` termina em `std::endl`, um flush por linha. O xlog escreve
   poucas linhas e não se importa; aqui a taxa é de dezenas a centenas de linhas/s, e um flush por
   linha viraria tempestade de syscall no mesmo laço que também drena o gravador do Tacview. Daí
   um `std::ofstream` próprio com `tick(dt)` de flush periódico (`flushEvery:`).
7. **`'suppressed'` (no `msgHealth`) só conta para mensagem de EVENTO.** Numa mensagem periódica
   o `every:` é um limitador de taxa — não emitir a cada ciclo é o comportamento pedido, e contar
   isso como supressão afogaria o número que interessa (a borda de evento adiada sob saturação).
8. **`'sinks:'`/`'messages:'` ficam em SLOT, nunca em `components:`.** É o que garante que
   `Component::updateTC()` — que só desce para a lista de componentes — não tenha caminho até
   eles (`MsgFeed` não sobrescreve `updateTC()`; o custo no frame de tempo crítico é
   estruturalmente zero, não uma promessa). O preço é encaminhar `reset()`/`shutdownNotification()`
   à mão, feito em `MsgFeed.cpp`.
9. **A amostragem não pode morar no `Player`** (seria o padrão de `models::CollisionDetect`, a
   50 Hz) — `Player::updateTC()`/`updateData()` são os dois guardados por
   `mode == ACTIVE || PRE_RELEASE`, e `crashNotification()` faz `setMode(CRASHED)`: um observador
   ali emudeceria exatamente na borda que existe para reportar. `MsgFeed` amostra de
   `Station::updateData()` em vez disso — preço aceito: resolução limitada ao passo do laço que
   chama `station->updateData()` (10 Hz em tempo real, 50 Hz em `-deterministic`).

## Testes

`tests/domain/test_xmsg_rules.cpp` (as quatro regras de `rules/`, sem `Station`) e
`tests/scenario/run_scenario_test.py`/`tests/memory/run_leak_test.py` (o `msgFeed:` de produção
rodando de ponta a ponta contra o binário de verdade).
