# `flight` — notas de arquitetura

Complementa [../README.md](../README.md) (visão geral, as quatro camadas, o `#ifdef` que separa
`libflight.so`/`libflight_tc.so`). Este documento junta as decisões de calibração e as armadilhas
específicas deste modelo que, de outra forma, só existiam espalhadas em
[../../../CLAUDE.md](../../../CLAUDE.md) — útil para quem abriu só `models/flight/` (ver o
`Makefile` ao lado para o build autocontido) e não tem o resto do repositório em mente.

## A aeronave é dado do MODELO, não do cenário

`data/jsbsim/` (o c310) e `configs/flight_tree.xml` são publicados junto com o `.so`
(`install_subdir()`/`install_data()` no `meson.build`, para `<prefix>/share/mixr-plugins/flight/`)
— não moram em `src/poc/<poc>/data/` de nenhuma poc. Não é só arrumação: `domain/`/`bt/` deste modelo
são calibrados **para o c310** especificamente:

- `maxClimbRateMps`/`maxRateOfTurnDps` do `Autopilot` (ver `include/domain/`).
- A folga de `domain/TerrainFloor.hpp` contra o piso anti-CFIT — o C310 desce ~330 m por
  engajamento com `maxClimbRateMps: 8.0` e `evadeHold: 30 s`; a folga do cenário (`terrainClearance`)
  tem de ficar dentro dessa margem do AGL de cruzeiro, ou o piso nunca é alcançado.
- Os limiares de combustível em `bt/nodes/FuelLowCondition.cpp`.

Trocar de aeronave sem recalibrar essas três coisas não faria sentido — por isso a aeronave viaja
**com** o modelo, não com o cenário.

## As quatro camadas — direção de dependência

Ver o diagrama em [../README.md](../README.md). O ponto que vale repetir: `bt/` não inclui nada de
`ubf/`, nada do MIXR e nada de `xnative/`, e o namespace (`bt_nodes`) fica deliberadamente fora de
`mixr::`. É o que permite `tests/tree/` carregar o `configs/flight_tree.xml` **de produção** contra
um `FakeDecisionContext`, em ~10 ms, sem linkar uma lib do MIXR — a suite mais barata e mais
repetida durante o desenvolvimento da árvore.

## O log do modelo (`LOG(...)` em `ubf/FlightAction.cpp`)

`shared/xlog` é uma `shared_library()`, então há **uma cópia no processo** e o `LOG(...)` emitido
de dentro deste `.so` (aberto por `dlopen`) cai no mesmo buffer/arquivo do host — é o que faz a aba
"Log" (F5) do `./app` mostrar o que o modelo registra, sem nenhuma ponte. Nas outras pocs
(`single-thread`/`multi-thread`), que não têm aba, as mesmas linhas saem no console e no
`data/logs/*.log`. Sob `-deterministic` o `main.cpp` chama `setLoggingEnabled(false)` e nada é
emitido — os dumps comparáveis não mudam.

`FlightAction::execute()` é o ponto certo para isso: é a única atuação comum aos dois agentes
(`SimAgent` de background e `FlightAgentTC` do pool T/C) e já é onde o rótulo vencedor chega. O
que se registra: transição de comportamento (`PATROL -> EVADE`), alerta tático, lançamento de
míssil (e as duas formas de ele não acontecer) e um batimento a cada `kHeartbeatEveryDecisions`
decisões atuadas.

**A armadilha, medida e corrigida:** `execute()` roda **até 50 Hz por aeronave**, e nem todo campo
da ação é um evento — `broadcast` (o pedido de alerta tático) fica **ligado** enquanto a aeronave
evade. Logar direto no `if (broadcast)` deu ~50 linhas/s por aeronave: em 20 s de intercepto o
buffer de 500 entradas já tinha girado três vezes e engolido justamente as transições. Por isso
todo log daqui passa por uma regra de **borda** (`changedFor()`, um mapa estático por player, com
mutex porque os agentes decidem em paralelo) ou de **contagem** (o batimento), nunca por decisão.

## Armadilhas confirmadas — não redescobrir

1. **O motor leva segundos para subir.** `JSBSimModel` nativo nunca liga o motor sozinho; quem
   liga é `data/jsbsim/systems/engine-autostart.xml`. Qualquer regra sobre empuxo/RPM precisa de
   um `hold:`/debounce maior que esse transiente (ver `shared/xmsg` no `CLAUDE.md` para a medição:
   ~7 s até ~400 lbf / 2700 RPM).
2. **`engRpmRaw` é polimórfico na unidade** entre motor a pistão (RPM absoluto), turbina (`%N2`) e
   turboprop (`%N1`) — o c310 é `engIO470D`, pistão, `maxrpm 2625`; não assumir percentual.
3. **O piso anti-CFIT depende de `terrainElevReq: false`** (default) — com `true`,
   `Player::updateElevation()` pula a consulta ao banco SRTM e espera um gerador externo. Ver a
   seção "Terreno" do `CLAUDE.md` para a lista completa (10 armadilhas) sobre `SrtmHgtFile`.
4. **`wrap180()` tem borda em -180, não em +180** (`fmod(360,360)==0`) — `domain/ThreatPolicy`
   escolhe o lado da quebra por `relBearingDeg >= 0`, então um contato exatamente a ré quebra
   sempre para o mesmo lado. Comportamento travado por teste, não coincidência.

## Testes

`tests/{domain,tree,native}/` — ver [../README.md](../README.md) para a contagem e o que cada
camada prova. `make test` (neste diretório, via o `Makefile` autocontido) roda as três.
