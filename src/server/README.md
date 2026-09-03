# src/server -- API REST de simulação

Recebe um **descritor de cenário** (texto EDL), roda a simulação num processo
isolado e devolve a telemetria final em JSON.

## Dois executáveis

- **`server`** -- a camada HTTP. Não linka `mixr_dep`. Recebe a requisição,
  valida o corpo, grava num diretório de trabalho próprio e dispara
  `sim-runner` como subprocesso (nunca via shell).
- **`sim-runner`** -- constrói **uma** `Station`, roda os frames pedidos e
  imprime **uma linha JSON** na stdout. Um processo por requisição -- ver o
  "porquê" em [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) e no `CLAUDE.md`
  raiz (`shared/xplugin` nunca teve hot-reload de plugin testado dentro de
  um processo vivo).

Nenhum dos dois é pensado para ser exposto sem `make install` primeiro
(precisam de `libflight_tc.so`/`flight_tree.xml`/dados JSBSim sincronizados
em `dist/`).

## Rodando

```bash
make configure && make sdk && make models && make build && make install
make run-server        # porta 8080 por default
```

Flags do `server` (`-port`, `-bind`, `-runner`, `-max-concurrent`,
`-timeout`) -- ver `include/app/ServerOptions.hpp`.

## Contrato HTTP

### `POST /simulate?frames=<N>&threads=<N>`

Corpo = texto EDL cru: **só o conteúdo que vai dentro de `players: {}`** --
não um `.epp` completo. O `server` já injeta um cabeçalho fixo (`Station`,
`WorldModel`, o `PluginLoader` do modelo `flight`, o terreno) antes do corpo
e o fecha depois (ver `configs/scenario_prefix.epp.in` +
`configs/scenario_suffix.epp.in` + `include/app/ScenarioAssembler.hpp`).

Cada player segue o mesmo molde de
`src/poc/multi-thread/configs/scenario.epp.in`: um agente
`( FlightAgentTC state: (FlightState) behavior: (UbfArbiter behaviors: {...}) )`
como **último** componente do player decide via árvore de comportamento;
sem esse bloco, o player só voa pelo `Autopilot` nativo (como o `bandit1`
das outras pocs). Exemplo mínimo, dois players:

```
alpha1: ( Aircraft
   id: 101
   side: blue
   type: "C310"
   signature: ( SigSphere radius: 3.0 )
   initXPos:     ( NauticalMiles 5.0 )
   initYPos:     ( NauticalMiles 0.0 )
   initAlt:      ( Meters 1750.0 )
   initHeading:  ( Degrees 90 )
   initVelocity: 82.0
   interpolateTerrain: true
   components: {
      dynamicsModel: ( JSBSimModel
         rootDir: "./dist/share/mixr-plugins/flight/jsbsim/"  model: "c310"  debugLevel: 0
      )
      pilot: ( Autopilot
         navMode: false  headingHoldMode: true  altitudeHoldMode: true
         velocityHoldMode: true  maxRateOfTurnDps: 3.0  maxBankAngle: 30.0
         maxPitchAngle: 10.0  maxClimbRateMps: 8.0  maxAcceleration: 2.0
      )
      datalink: ( AlertDatalink holdTime: ( Seconds 25 ) )
      antennas: ( Gimbal components: { radar: ( Antenna
         polarization: horizontal  gain: ( dB 42 )
         gainPattern: ( Func1 table: ( Table1
            x:    [ 0.0   0.01745  0.02618   0.04363   0.05236   0.061087  0.06981   0.07854 ]
            data: [ 0.0  -3.0    -10.0     -30.0     -20.0     -14.0     -25.0     -80.0 ] ) )
         gainPatternDeg: false
         initPosition: [ 0 0 ]  maxRates: [ 0.8727 0.8727 ]
         commandRateAzimuth: ( Degrees 60 )  commandRateElevation: ( Degrees 60 )
         reference: [ 0 0 ]  searchVolume: [ 1.0472 0.05 ]  numBars: 2
         maxPlayersOfInterest: 8  playerOfInterestTypes: { air }
         maxRange2PlayersOfInterest: ( NauticalMiles 40.0 )
         maxAngle2PlayersOfInterest: ( Degrees 60.0 )
         localPlayersOfInterestOnly: false  useWorldCoordinates: false
      ) } )
      sensors: ( SensorMgr components: { ( Tws
         trackManagerName: twsTrkMgr  antennaName: radar
         powerPeak: ( KiloWatts 200.0 )  frequency: ( GigaHertz 3.0 )
         bandwidth: ( GigaHertz 2.0 )  PRF: ( Hertz 500.0 )
         pulseWidth: ( MilliSeconds 0.01 )  threshold: ( dB 0.0 )  igain: ( dB 0 )
         ranges: [ 10 20 40 80 ]  initRangeIdx: 2 ) } )
      obc: ( OnboardComputer components: { twsTrkMgr: ( AirTrkMgr
         maxTracks: 20  alpha: 1.0  beta: 0.5  gamma: 0.0
         positionGate: 1500  rangeGate: 500  velocityGate: 10  firstTrackId: 1000 ) } )

      agent: ( FlightAgentTC
         state: ( FlightState )
         behavior: ( UbfArbiter behaviors: {
            ( AltitudeSafetyBehavior vote: 90
               minAltitude: ( Meters 1200 )  recoverAltitude: ( Meters 1750 )
               recoverSpeed: 160.0  minClearance: ( Meters 400 )  recoverClearance: ( Meters 800 ) )
            ( BtBehavior vote: 50
               treeFile: "./dist/share/mixr-plugins/flight/flight_tree.xml"
               patrolHeading: ( Degrees 90 )  legTime: ( Seconds 60 )  legTurn: ( Degrees 90 )
               patrolAltitude: ( Meters 1750 )  patrolSpeed: 160.0
               rtbAltitude: ( Meters 2050 )  rtbSpeed: 170.0
               arrivalRadius: ( NauticalMiles 2.0 )  fuelReserve: 0.35
               breakTurn: ( Degrees 110 )  evadeClimb: ( Meters 700 )  evadeSpeed: 185.0
               terrainClearance: ( Meters 800 )  evadeHold: ( Seconds 30 )  supportSpeed: 180.0 )
         } )
      )
   }
)

bandit1: ( Aircraft
   id: 201
   side: red
   type: "C310"
   signature: ( SigSphere radius: 4.0 )
   initXPos:     ( NauticalMiles 12.0 )
   initYPos:     ( NauticalMiles 12.0 )
   initAlt:      ( Meters 2400.0 )
   initHeading:  ( Degrees 225 )
   initVelocity: 92.0
   interpolateTerrain: true
   components: {
      dynamicsModel: ( JSBSimModel
         rootDir: "./dist/share/mixr-plugins/flight/jsbsim/"  model: "c310"  debugLevel: 0
      )
      pilot: ( Autopilot
         navMode: false  headingHoldMode: true  altitudeHoldMode: true
         velocityHoldMode: true  maxRateOfTurnDps: 3.0  maxBankAngle: 30.0
         maxPitchAngle: 10.0  maxClimbRateMps: 8.0  maxAcceleration: 2.0
      )
   }
)
```

- `frames`: opcional, inteiro em `[1, 6000]`, default `600`.
- `threads`: opcional, inteiro em `[1, 16]`.
- Corpo rejeitado (`400`) se: vazio, maior que 256 KB, contiver byte NUL, ou
  contiver `PluginLoader`, `PluginModule`, `networks:` ou `dataRecorder:`
  (ver **Fronteira de confiança** abaixo).

```bash
curl -s --data-binary @meu_cenario.epp "localhost:8080/simulate?frames=200" | jq .
```

### `GET /health`

`200 {"status":"ok"}`.

## Resposta

```json
{
  "framesRequested": 200, "framesRun": 200, "threads": 4, "simTimeSec": 4.0,
  "players": [
    {
      "name": "alpha1", "type": "C310", "side": "BLUE", "majorType": ["AIR_VEHICLE"],
      "mode": "ACTIVE",
      "position": { "northM": 9260.0, "eastM": 120.4, "latDeg": -22.19, "lonDeg": -42.47 },
      "altitudeM": 1751.2, "terrainElevationM": 905.0, "altitudeAglM": 846.2,
      "headingDeg": 91.3, "rollDeg": 2.1, "pitchDeg": 0.4,
      "speedMps": 160.2, "mach": 0.47, "fuelWtLbs": 512.0,
      "behaviorLabel": "PATROL", "decisions": 201,
      "track": { "found": true, "name": "bandit1", "rangeM": 21000.0, "relBearingDeg": -15.2 },
      "alert": { "active": false }
    }
  ]
}
```

Erros: `400` (validação), `422` (cenário não compilou -- `edl_parser` falhou;
`stderr` truncado vem no corpo), `503` (limite de simulações concorrentes),
`504` (timeout, processo morto com `SIGKILL`).

## Fronteira de confiança

O `server` roda um binário próprio (`sim-runner`) contra um cenário montado a
partir de texto enviado pelo cliente. A heurística de `ScenarioUpload.cpp`
(recusar `PluginLoader`/`PluginModule`/`networks:`/`dataRecorder:`) **não é
um parser EDL completo** -- é uma checagem de substring, barata e com
precedente no próprio repositório (as fixtures de teste já removem
`networks:` pelo mesmo motivo de hermeticidade). Ela impede o caso óbvio
(apontar `PluginLoader` para um `.so` arbitrário = `dlopen()` de código
nativo arbitrário), mas **não é uma sandbox**. Não exponha este endpoint
publicamente sem autenticação/isolamento adicional (usuário/contêiner
restrito para o `sim-runner`, por exemplo).

## Testes

`tests/unit/` (gtest, sem MIXR: a lista de bloqueio e a montagem do `.epp`) +
`tests/http/run_server_test.py` (contrato HTTP fim-a-fim, contra os binários
de verdade). Registrados na suíte `server` do host:

```bash
meson configure build -Dtests=true
make test                              # tudo, incluindo esta suíte
meson test -C build --suite server     # so esta suíte (precisa de 'make install' antes)
```

Ver [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) para o que cada camada prova e as armadilhas
já encontradas construindo isto.
