# A4-4DOF — o player máximo, dinâmica LaeroModel (4-DOF), navegando de verdade

Um dos **cinco** cenários da família `A4-*DOF` deste `sandbox/` — ver
`sandbox/A4-6DOF/README.md` para a tabela completa da família e a porta
Tacview compartilhada (**1234**, de propósito). Esta variante troca **só**
o `dynamicsModel:` em relação a `A4-6DOF`: tudo o resto (player, rota,
`agent:`/árvore) é byte a byte idêntico.

```bash
./build/app/src/app -folder ./sandbox -scenario A4-4DOF        # tempo real, Tacview 1234
./build/app/src/app -folder ./sandbox -scenario A4-4DOF -deterministic 200
```

## O que muda: `dynamicsModel: ( LaeroModel )`

```
dynamicsModel: ( LaeroModel )
```

`LaeroModel` **não tem SLOTTABLE nenhuma** (`EMPTY_SLOTTABLE`) — nenhum
argumento é aceito ou possível. É documentado pelo próprio framework como
"4 degree of freedom aerodynamic model" (`LaeroModel.hpp`) — ao contrário de
`RacModel` (`A4-3DOF`), que não tem contagem de DOF formal nenhuma.

**Por que o `( Navigate )`/`Autopilot` continuam funcionando sem mudança
nenhuma**: `Autopilot::headingController/altitudeController/
velocityController` chamam `DynamicsModel::setCommandedHeadingD/Altitude/
VelocityKts` **incondicionalmente**, independente de qual subclasse está
anexada (confirmado lendo `contexts/src/mixr/src/models/system/
Autopilot.cpp`). `LaeroModel` implementa os três sem nenhuma condição extra
(ao contrário do `JSBSimModel`, que só engata se o airframe JSBSim
específico tiver seu próprio FCS de piloto automático — ver a nota "F4N" em
`CLAUDE.md`, seção "Gotchas de unidades e de modelo"). Confirmado também: os
parâmetros secundários do `Autopilot` (`maxRateOfTurnDps`/`maxBankAngle`/
`maxPitchAngle`/`maxClimbRateMps`, herdados sem mudança) **são usados de
verdade** pelo `LaeroModel` (via `flyPhi`/`flyTht` internos) — ao contrário
do `RacModel` de `A4-3DOF`, onde os mesmos slots ficam inertes.

`type: "A4"` no player continua só um rótulo/ícone do Tacview aqui — o
`LaeroModel` não carrega tabela aerodinâmica nenhuma por tipo de aeronave
(ao contrário do `JSBSimModel`, que lê `data/jsbsim/aircraft/A4/`), então a
dinâmica de voo em si é genérica, não calibrada especificamente para o A-4.

## Achado medindo: telemetria de combustível/Mach lê zero, e está certo

Rodando `-deterministic 200`, o dump mostra `fuel=0.000000000` e
`mach=0.000000000` o tempo todo — **não é um defeito desta variante**. O
campo `fuel=` do dump (`app/src/app/DeterministicDump.cpp`) lê
`AirVehicle::getFuelWt()` direto, que só é populado por um `DynamicsModel`
que simula tanque de combustível de verdade (`JSBSimModel`, via os dados de
`data/jsbsim/`); `LaeroModel` não modela isso, então o valor cru fica em 0.
**Isso não afeta a decisão do UBF**: `domain::WorldView::fuelFraction`
(usado pela condição `FuelLow` nas árvores que a têm) é calculado em
`FlightState.cpp` com uma guarda explícita —
`(fuelMax > 0.0) ? (getFuelWt()/fuelMax) : 1.0` — então sem tanque simulado
ele cai para `1.0` (tanque cheio, nunca baixo), não para `0.0`. Mach segue o
mesmo padrão (campo específico do FDM do JSBSim, sem equivalente em
`LaeroModel`).

## Vocabulário extra do Steerpoint: `sca`/`magvar`/`pta`

Ver `sandbox/A4-6DOF/README.md` para o detalhe completo — os 4 `Steerpoint` desta
rota (idêntica à de `A4-6DOF`) ganharam os mesmos três slots nativos ociosos
(`sca`/`magvar`/`pta`), recalculados todo frame por `Steerpoint::compute()` mas
sem consumidor neste repositório. `stptType` continua nos mesmos 4 valores
(`DEST`/`TGT`/`FIX`/`IP`), pelo mesmo motivo.

## Verificação manual

```bash
python3 src/ui/scripts/edl_lint.py sandbox/A4-4DOF/configs/scenario_a4_4dof.edl.in
dist/bin/edlcheck sandbox/A4-4DOF/configs/scenario_a4_4dof.edl.in   # ver a nota do @NUM_TC_THREADS@ no README de A4-6DOF
./build/app/src/app -folder ./sandbox -scenario A4-4DOF -deterministic 200 > /tmp/a4-4dof.log 2>&1
grep -o 'bt=[A-Za-z_-]*' /tmp/a4-4dof.log | sort -u   # esperado: so bt=NAV
./tests/determinism/check_determinism.sh ./build/app/src/app A4-4DOF 2000 '' \
    sandbox/A4-4DOF/configs/scenario_a4_4dof.edl.in
```

## O que herda sem mudança

Tudo de `A4-6DOF` exceto o `dynamicsModel:` — os 53 componentes, a rota de 4
steerpoints com as 4 `Action` disparando a cada volta, `pilot:`/`Autopilot`,
`agent:`/`flight_tree_nav.xml`, terreno, `provides:`. Ver
`sandbox/A4-6DOF/README.md` e `src/poc/built-in_mixr_1/README.md`.
