# built-in_mixr_1 — o player máximo

Responde a **uma** pergunta: *qual o player mais elaborado que dá para montar
usando o máximo de componentes built-in do framework?*

A resposta é `falcon1` de [configs/scenario_max_player.epp.in](configs/scenario_max_player.epp.in):
um único `( Aircraft )` com **53 das 96 classes** que `mixr::models::factory`
publica. A única peça não nativa dele é o `( AlertDatalink )`, que herda de
`models::Datalink` só para decidir o que fazer com a mensagem recebida.

```bash
make run-built-in_mixr_1            # tempo real, Tacview na porta 1239
make check-built-in_mixr_1          # determinismo com 1, 2 e 4 threads T/C

# quem executa e o ./app, o runner unico -- esta poc nao tem binario proprio
./build/app/src/app -scenario built-in_mixr_1 -deterministic 30000
```

**Esta poc não tem código nenhum** — só o cenário, os dados de execução e este
README; quem a executa é o `./app`, o runner único de todas as pocs. Ela não
acrescentou uma linha de C++, nem um nome de fábrica, nem recompilou o modelo.
**A diferença dela é 100% de cenário.**

## O teto é estrutural: dez sistemas primários, um de cada

`Player::updateSystemPointers()` resolve dez ponteiros com `findByType()`, que
devolve o **primeiro** casamento — um segundo `( Navigation )` irmão seria
invisível. Pluralidade só existe onde o framework deu um **contêiner**:

| # | tipo primário | o que `falcon1` põe lá |
|---|---|---|
| 1 | `DynamicsModel` | `JSBSimModel` |
| 2 | `Pilot` | `Autopilot` |
| 3 | `Navigation` | `Ins` ← `Gps`; `Route` com 4 `Steerpoint` + as **4** `Action*`; `Bullseye` |
| 4 | `Datalink` | `AlertDatalink` *(única peça não nativa)* |
| 5 | `Radio` | `CommRadio` ← `Iff` |
| 6 | `Gimbal` | `Gimbal` ← 6 `Antenna` + `StabilizingGimbal` + `IrSeeker` |
| 7 | `RfSensor` | `SensorMgr` ← `Tws` `Stt` `Sar` `Gmti` `Rwr` `Jammer` |
| 8 | `IrSystem` | `IrSensor` |
| 9 | `OnboardComputer` | `AirTrkMgr` `RwrTrkMgr` `GmtiTrkMgr` `AirAngleOnlyTrkMgr` |
| 10 | `StoresMgr` | 11 estações: `AamMissile`×2 `AgmMissile` `Sam` `Bomb` `Chaff` `Flare` `Decoy` `Gun`+`Bullet` `FuelTank` `AvionicsPod` |

Fora dos dez: `CollisionDetect` (Component comum — cabem quantos quiser) e as
assinaturas — `SigSwitch` com as **seis** classes de RCS como filhos, comutadas
em runtime pelo `camouflageType`, mais `IrSignature` + `IrSphere`.

**Três regras que o desenho obedece**, todas lidas no fonte do framework:

1. `Iff` **é** um `Radio` e `Gps` **é** uma `Navigation` — por isso vão
   **aninhados**, não como irmãos: irmãos disputariam o mesmo ponteiro primário.
2. **Uma antena por sensor de RF.** `Antenna::setSystem()` guarda um único
   ponteiro; dois sensores no mesmo `antennaName:` fazem o último a dar
   `reset()` vencer, em silêncio. Daí as seis antenas.
3. Sensor→antena e sensor→trackmanager casam **por nome de slot**, e
   `Component::findByName()` é recursivo — uma antena pendurada num gimbal
   interno continua alcançável por nome simples.

## O que foi medido rodando (30.000 frames)

* **Zero** erro de parse e **zero** `was not found!` — as 6 antenas, os 4 track
  managers e o seeker casam todos.
* A cadeia de produção continua idêntica: `falcon1` detecta `bandit1` no TWS,
  vai para `EVADE`, propaga o alerta, `falcon2/3/4` vão para `SUPPORT`. Nenhum
  sistema acrescentado interfere.
* A rota sequenciou os **quatro** steerpoints numa volta: decoy solto em
  **t=37,0 s** (wp1, `ActionDecoyRelease`) e bomba solta em **t=225,3 s** (wp4,
  `ActionWeaponRelease`), ambos com ciclo `preRelease → active → detonated`,
  visíveis no `MsgFeed` e no Tacview.
* **Determinismo preservado**: dumps `frame=` byte-idênticos com 1, 2 e 4
  threads de tempo crítico.

## Armadilhas confirmadas — não redescobrir

1. **`AircraftIrSignature` derruba o processo** se declarada sem as 6 tabelas —
   `getAirframeSignature()` desreferencia `airframeSignatureTable` sem checar
   nulo. Por isso aqui é `( IrSignature )` simples.
2. **`MergingIrSensor` é um beco sem saída neste fork**: exige
   `AirAngleOnlyTrkMgrPT`, cujo header é incluído em `models/factory.cpp` mas
   **não tem branch** — avisa a cada `reset()` e não há saída pelo EDL.
3. **`ActionWeaponRelease` ignora o slot `station`** — sempre chama
   `releaseOneBomb()`; quem sai é a primeira `( Bomb )` livre.
4. **`ActionDecoyRelease` conta o `interval` em relógio de PAREDE**
   (`getSimTimeOfDay()`), não em tempo simulado: em `-deterministic` um
   `( Seconds 1.0 )` virou ~190 s de tempo simulado entre um decoy e o seguinte.
5. **`Table2`**: o `data:` é **lista de listas** (`{ [...] [...] }`), uma
   sublista por ponto de `y` — a lista plana falha com *"Data table aborted"*.
6. **`sarLatitude`/`targetLatitude` são `base::LatLon`**, não `base::Angle`:
   `( LatLon direction: "s" degrees: 22 minutes: 12 )`.
7. **`dataLogTime:` em cada store liberável** — sem ele o flyout nasce, voa e
   detona sem nunca aparecer no Tacview (a armadilha 1 do `xtacview` vale igual
   para arma liberada). O `MsgFeed` enxerga assim mesmo, porque lê o player
   direto em vez de passar pelo gravador.
8. **As `( Action )` só disparam se a aeronave PASSAR pelos steerpoints**, e
   quem pilota é a árvore de comportamento (`navMode: false`) — assim que há
   contato ela abandona o circuito. Daí o `bandit1` a 30 NM: dá tempo de fechar
   a volta antes do primeiro contato.

## Por que o cenário não se chama `scenario.epp.in`

`tests/guard/check_falcons_estrutura.sh` varre esse nome por glob e exige que
`falcon1..4` tenham o **mesmo esqueleto de slots**. Aqui `falcon1` é
propositalmente diferente dos outros três — que ficam na pilha de produção,
servindo de contraste e de alvo para o RWR dele. O arquivo tem nome próprio,
mesmo recurso que a `bandit` já usa (o cenário dela é um `scenario.epp`).

Pelo mesmo motivo esta poc **não entra na lista `pocs`** de
[tests/meson.build](../../../tests/meson.build): as suítes `scenario`/`memory`
derivam fixtures de `configs/scenario.epp.in` via `make_fixture.py`. O
determinismo tem alvo próprio (`make check-built-in_mixr_1`), que roda contra o
cenário desta pasta — já hermético, sem `networks:`.

## Das 96 classes, o que sobra e por quê

Das 43 não usadas: **20 são outros tipos de player** (Tank, Ship, Building,
Helicopter…), **3 são do cenário** (`WorldModel`, `IrAtmosphere`,
`IrAtmosphere1`), **11 são classes-base substituídas pela subclasse já em uso**
(`Radar` por `Tws`, `Radio` por `CommRadio`, `Stores` por `StoresMgr`…), **2 são
objetos de runtime** (`Track`, `TargetData`), **2 vivem na `Station`**
(`SimAgent`, `MultiActorAgent`) e **5 são alternativas mutuamente exclusivas** ao
que já ocupa o slot único (`RacModel`/`LaeroModel` contra `JSBSimModel`, `IrBox`
contra `IrSphere`) ou inutilizáveis (armadilhas 1 e 2 acima).

**53 é o teto real** para um único player.
