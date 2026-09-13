# `tests/fixtures/` — cenários de referência que não são mais pocs

Duas pastas, cada uma um cenário EDL completo (`configs/` + `data/`, sem
código próprio — o padrão de qualquer poc deste repositório), que **não
aparecem mais em `src/poc/`**: `built-in_mixr_1` e `full-systems-nav` foram
removidas como pocs (ver `TODO.md`/`CLAUDE.md`), mas os `.edl.in` continuam
sendo consumidos de verdade por outras partes do repositório:

| quem usa | o quê |
|---|---|
| `src/ui/scripts/build.js` (`make open-edl`) | `built-in_mixr_1/configs/scenario_max_player.edl.in` é a fonte do preset "carregar exemplo" do editor gráfico — o cenário com mais componentes do repositório, usado para provar que a árvore de cartões/pendências escala |
| `tests/tools/test_edl_lint.py`, `test_edlcheck.py`, `test_edl_catalog.py` | os dois `.edl.in` entram nas listas `REAL_SCENARIOS`/equivalente — arquivos reais varridos para validar o catálogo/lint contra o fonte atual |
| `tests/scenario/run_scenario_folder_test.py` | deriva cópias de `full-systems-nav/configs/scenario_full_nav.edl.in` (cenário real de **um** player só, chamado `a4`) para testar `-folder <pasta>` ponta a ponta |
| `tests/scenario/run_dual_tacview_port_test.py` | deriva duas cópias do mesmo `.edl.in`, para testar dois processos disputando a mesma porta Tacview |
| `tests/scenario/run_app_stress_test.py` | `built-in_mixr_1` é o cenário default do teste de estresse do `./app` (`-folder tests/fixtures -scenario built-in_mixr_1`) |

Continuam executáveis direto, como qualquer cenário de `-folder`:

```bash
./build/app/src/app -folder tests/fixtures -scenario built-in_mixr_1
./build/app/src/app -folder tests/fixtures -scenario full-systems-nav
```

Nenhuma delas entra na lista `pocs` de `tests/meson.build` nem em
`tests/guard/check_falcons_estrutura.sh` (que varre só `src/poc/**/configs/
scenario.edl.in` — os dois arquivos aqui têm nome próprio, de propósito, e
hoje nem moram mais sob `src/poc/`).

## `built-in_mixr_1` — o player máximo

Responde a **uma** pergunta: *qual o player mais elaborado que dá para
montar usando o máximo de componentes built-in do framework?*

A resposta é `falcon1`: um único `( Aircraft )` com **53 das 96 classes** que
`mixr::models::factory` publica. A única peça não nativa é o
`( AlertDatalink )`, que herda de `models::Datalink` só para decidir o que
fazer com a mensagem recebida.

`Player::updateSystemPointers()` resolve dez ponteiros com `findByType()`,
que devolve o **primeiro** casamento — um segundo `( Navigation )` irmão
seria invisível. Pluralidade só existe onde o framework deu um **contêiner**:

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

Fora dos dez: `CollisionDetect` (Component comum) e as assinaturas —
`SigSwitch` com as **seis** classes de RCS como filhos, comutadas em runtime
pelo `camouflageType`, mais `IrSignature` + `IrSphere`.

**Três regras que o desenho obedece**, lidas no fonte do framework:

1. `Iff` **é** um `Radio` e `Gps` **é** uma `Navigation` — vão **aninhados**,
   não como irmãos: irmãos disputariam o mesmo ponteiro primário.
2. **Uma antena por sensor de RF.** `Antenna::setSystem()` guarda um único
   ponteiro; dois sensores no mesmo `antennaName:` fazem o último a dar
   `reset()` vencer, em silêncio. Daí as seis antenas.
3. Sensor→antena e sensor→trackmanager casam **por nome de slot**, e
   `Component::findByName()` é recursivo.

**53 é o teto real** para um único player: das 43 classes não usadas, 20 são
outros tipos de player (Tank, Ship, Building, Helicopter…), 3 são do cenário,
11 são classes-base substituídas pela subclasse já em uso, 2 são objetos de
runtime (`Track`, `TargetData`), 2 vivem na `Station` (`SimAgent`,
`MultiActorAgent`) e 5 são alternativas mutuamente exclusivas ao que já
ocupa o slot único.

### Armadilhas confirmadas — não redescobrir

1. **`AircraftIrSignature` derruba o processo** se declarada sem as 6
   tabelas — `getAirframeSignature()` desreferencia `airframeSignatureTable`
   sem checar nulo. Por isso aqui é `( IrSignature )` simples.
2. **`MergingIrSensor` é um beco sem saída neste fork**: exige
   `AirAngleOnlyTrkMgrPT`, cujo header é incluído em `models/factory.cpp` mas
   **não tem branch** — avisa a cada `reset()` e não há saída pelo EDL.
3. **`ActionWeaponRelease` ignora o slot `station`** — sempre chama
   `releaseOneBomb()`; quem sai é a primeira `( Bomb )` livre.
4. **`ActionDecoyRelease` conta o `interval` em relógio de PAREDE**
   (`getSimTimeOfDay()`), não em tempo simulado: em `-deterministic` um
   `( Seconds 1.0 )` virou ~190 s de tempo simulado entre um decoy e o
   seguinte.
5. **`Table2`**: o `data:` é **lista de listas**, uma sublista por ponto de
   `y` — a lista plana falha com *"Data table aborted"*.
6. **`sarLatitude`/`targetLatitude` são `base::LatLon`**, não `base::Angle`.
7. **`dataLogTime:` em cada store liberável** — sem ele o flyout nasce, voa
   e detona sem nunca aparecer no Tacview.
8. **As `( Action )` só disparam se a aeronave PASSAR pelos steerpoints**, e
   quem pilota é a árvore de comportamento (`navMode: false`) — assim que há
   contato ela abandona o circuito. Daí o `bandit1` a 30 NM: dá tempo de
   fechar a volta antes do primeiro contato.

Medido rodando (30.000 frames, `-deterministic`): zero erro de parse e zero
`was not found!`; a cadeia de produção intacta (`falcon1` detecta `bandit1`
no TWS, `EVADE`, alerta propaga, `SUPPORT`); a rota sequenciou os quatro
steerpoints numa volta (decoy em t=37,0 s, bomba em t=225,3 s); dumps
`frame=` byte-idênticos com 1, 2 e 4 threads T/C.

## `full-systems-nav` — o mesmo player máximo, navegando de verdade

Reusa os mesmos 53 componentes de `built-in_mixr_1` num único player (`a4`),
mas troca a árvore de comportamento: `flight_tree_nav.xml` tem **um nó só**,
`( Navigate )`, sem `Patrol`/`RTB`/`Evade` por baixo. Em vez da árvore de
produção pilotar via patrulha geométrica (o `airspeed:` de cada steerpoint
sendo só metadado decorativo), aqui `( Navigate )` **lê de verdade**
`Route`/`Steerpoint` nativos (`Navigation::getTrueBrgDeg()`,
`Steerpoint::getCmdAltitudeM()`/`getCmdAirspeedKts()`) e entrega isso como
comando pelo mesmo caminho de atuação que `Patrol`/`RTB`/`Evade` usam
(`FlightAction::execute()` → `Autopilot`).

**Armadilha medida, não redescobrir**: os `airspeed:` de cada `Steerpoint`,
copiados sem pensar de `built-in_mixr_1` (160-170 kt, lá só decorativo),
causavam CFIT contínuo — comandado a manter altitude E 160 kt ao mesmo
tempo (bem abaixo da faixa de voo nivelado deste `JSBSimModel`, 350-420 kt),
o avião não conseguia as duas coisas e as duas caíam juntas. Corrigido para
350-370 kt; 30.000 frames sem cair uma vez.

`( Navigate )` tem um limitador de taxa no rumo comandado
(`kMaxHeadingRateDegPerSec`, `bt/nodes/NavigateAction.cpp`) pela mesma razão
que qualquer guiagem só-proporcional precisa de amortecimento — mas o
limitador **não** foi a causa do CFIT acima (isolado rodando com o rumo
travado em 90° fixo: a mesma queda, idêntica).

Medido rodando (30.000 frames, `-deterministic`): `bt=NAV` em 100% das
linhas; a aeronave completa a volta repetidamente (`wrap: true`), altitude e
velocidade convergindo por `Steerpoint` (1750/1900/1750/1600 m,
350/370/350/360 kt); `ActionWeaponRelease` do steerpoint 4 dispara de
verdade (`W10001` no `MsgFeed`/Tacview); dumps `frame=` byte-idênticos com
1, 2 e 4 threads T/C.

Herda de `built-in_mixr_1`, sem mudança: as oito armadilhas de montagem
acima, todas valendo aqui também.
