# A4-4DOF — oito players máximos empilhados, dinâmica LaeroModel (4-DOF), voando uma figura-de-oito

Um dos **cinco** cenários da família `A4-*DOF` deste `sandbox/` — ver
`sandbox/A4-6DOF/README.md` para a tabela completa da família e a porta
Tacview compartilhada (**1234**, de propósito). Esta variante troca **só** o
`dynamicsModel:` em relação a `A4-6DOF`.

```bash
./build/app/src/app -folder ./sandbox -scenario A4-4DOF        # tempo real, Tacview 1234
./build/app/src/app -folder ./sandbox -scenario A4-4DOF -deterministic 600
```

## "Idêntico a A4-6DOF exceto o `dynamicsModel:`" — e como conferir

O `.edl.in` desta pasta é o de `A4-6DOF` **linha a linha**: a frota de **oito**
aeronaves idênticas (`a4_1`..`a4_8`, empilhadas com 1.000 ft de separação
vertical), a **figura-de-oito de 20 steerpoints** com o perfil de altitude
entre 4.000 e 15.000 ft (11.000 a 22.000 ft na mais alta), os 53 componentes
de cada player, `pilot:`/`Autopilot`, `agent:`/árvore, terreno e `provides:`.
Fora do bloco `dynamicsModel:` mudam apenas os caminhos/rótulos que carregam o
nome desta pasta (`data/`, `callsign`, `eventName`) e o cabeçalho de
comentário. Não é uma promessa — é verificável:

```bash
diff <(sed 's/A4-4DOF/A4-6DOF/g' sandbox/A4-4DOF/configs/scenario_a4_4dof.edl.in | grep -v '^\s*//') \
     <(grep -v '^\s*//' sandbox/A4-6DOF/configs/scenario_a4_6dof.edl.in)
```

Medido: o diff acima é **exatamente** os 8 blocos `dynamicsModel:`, nada mais.

> **Isto é uma mudança recente.** Antes, esta pasta ainda tinha **um** player
> `a4` e a rota original de 4 steerpoints, enquanto `A4-6DOF` já tinha
> evoluído para a frota de oito e a figura-de-oito — o README daqui afirmava
> "byte a byte idêntico" sem que fosse verdade. Agora é.

## O que muda: `dynamicsModel: ( LaeroModel )`

```
dynamicsModel: ( LaeroModel )
```

Uma linha, igual nas oito aeronaves: `LaeroModel` **não tem SLOTTABLE
nenhuma** (`EMPTY_SLOTTABLE`, `LaeroModel.cpp:22`) — nenhum argumento é
aceito ou possível. É documentado pelo próprio framework como "4 degree of
freedom aerodynamic model" (`LaeroModel.hpp`) — ao contrário de `RacModel`
(`A4-3DOF`), que não tem contagem de DOF formal nenhuma.

**Por que o `( Navigate )`/`Autopilot` continuam funcionando sem mudança
nenhuma**: `Autopilot::headingController/altitudeController/
velocityController` chamam `DynamicsModel::setCommandedHeadingD/Altitude/
VelocityKts` **incondicionalmente**, independente de qual subclasse está
anexada (confirmado lendo `contexts/src/mixr/src/models/system/
Autopilot.cpp`). `LaeroModel` implementa os três sem nenhuma condição extra
(ao contrário do `JSBSimModel`, que só engata se o airframe JSBSim específico
tiver seu próprio FCS de piloto automático — ver a nota "F4N" em `CLAUDE.md`,
seção "Gotchas de unidades e de modelo"). Confirmado também: os parâmetros
secundários do `Autopilot` (`maxRateOfTurnDps`/`maxBankAngle`/
`maxPitchAngle`/`maxClimbRateMps`, herdados sem mudança) **são usados de
verdade** pelo `LaeroModel` (via `flyPhi`/`flyTht` internos) — ao contrário
do `RacModel` de `A4-3DOF`, onde os mesmos slots ficam inertes, e ao
contrário do `JSBSimModel`, que os descarta em favor do autopiloto do próprio
airframe.

`type: "A4"` no player continua só um rótulo/ícone do Tacview aqui — o
`LaeroModel` não carrega tabela aerodinâmica nenhuma por tipo de aeronave (ao
contrário do `JSBSimModel`, que lê `data/jsbsim/aircraft/A4/`), então a
dinâmica de voo em si é genérica, não calibrada especificamente para o A-4.

**O raio de curva muda de dono, e encolhe.** O comentário da rota (herdado)
explica que as pernas de 14 km foram dimensionadas contra os ~4,8 km de raio
do autopiloto do airframe JSBSim. Aqui quem manda é `maxRateOfTurnDps: 6.0`
do `( Autopilot )`, que a 135 m/s dá ~1,3 km — **calculado pela fórmula, não
medido em voo**. A perna dimensionada para o caso mais folgado continua com
margem de sobra; a antecipação de `autoSeqDistance: 2.0 NM` passa a cortar
mais canto do que o necessário, sem prejuízo para o sequenciamento.

## Consequência herdada: o `airspeed:` calibrado dos steerpoints não vale aqui

Cada steerpoint de `A4-6DOF` carrega uma velocidade **calibrada** recalculada
por aeronave, porque o autopiloto do airframe JSBSim fecha a malha contra
`velocities/vc-kts` — é o que segura a pilha de oito junta. `LaeroModel`
fecha contra `Player::getTotalVelocityKts()` (`LaeroModel.cpp:573`), ou seja
velocidade **verdadeira**, então essa compensação por altitude não tem para
quem valer: as oito comandam verdadeiras ligeiramente diferentes entre si e a
pilha se estica ao longo da rota. **Os números foram mantidos idênticos de
propósito** — a única diferença pedida para este cenário é o
`dynamicsModel:`. Para colar a formação sob este modelo, os vinte
`airspeed:` teriam de voltar a ser o mesmo valor (262) nas oito; é mudança de
cenário, não desta variante.

Efeito medido numa volta (`-deterministic 118000`): a `a4_1` desce a **75,5
m/s** no ponto mais lento (contra os 135 m/s nominais do `A4-6DOF`) e a volta
não fecha dentro dos 2.360 s simulados — para a ~12 km do ponto de partida.
Vale observar antes de tirar conclusão de desempenho comparando os três
cenários da família: parte da diferença vem daqui, não do modelo em si.

## Achado medindo: telemetria de combustível/Mach lê zero, e está certo

O dump mostra `fuel=0.000000000` e `mach=0.000000000` o tempo todo — **não é
um defeito desta variante**. O campo `fuel=` (`app/src/app/
DeterministicDump.cpp`) lê `AirVehicle::getFuelWt()` direto, que só é
populado por um `DynamicsModel` que simula tanque de combustível de verdade
(`JSBSimModel`, via os dados de `data/jsbsim/`); `LaeroModel` não modela
isso, então o valor cru fica em 0. **Isso não afeta a decisão do UBF**:
`domain::WorldView::fuelFraction` (usado pela condição `FuelLow` nas árvores
que a têm) é calculado em `FlightState.cpp` com uma guarda explícita —
`(fuelMax > 0.0) ? (getFuelWt()/fuelMax) : 1.0` — então sem tanque simulado
ele cai para `1.0` (tanque cheio, nunca baixo), não para `0.0`. Mach segue o
mesmo padrão (campo específico do FDM do JSBSim, sem equivalente em
`LaeroModel`).

## Medido rodando

`-deterministic 600`: `bt=NAV` em 100% das linhas, as **oito** aeronaves
presentes no dump, zero erro de parse, zero `was not found!`.

`-deterministic 118000` (uma volta, ~2.360 s simulados): perfil de altitude
cumprido **exatamente** — `a4_1` entre **4.000 e 15.000 ft** e `a4_8` entre
**11.000 e 22.000 ft**, sem *overshoot* mensurável no arredondamento a pé —
e **AGL mínimo de 1.176 m** sobre toda a frota: ninguém chega perto do
terreno. A trajetória cobre os dois lobos do oito (~71 km em norte, ~82 km em
leste).

Determinismo: `check_determinism.sh` passa com **1, 2 e 4 threads** T/C,
dumps byte-idênticos, mais a repetição de 4 threads.

## Vocabulário extra do Steerpoint: `sca`/`magvar`/`pta`

Ver `sandbox/A4-6DOF/README.md` — os 20 `Steerpoint` desta rota (idêntica à
de `A4-6DOF`) carregam os mesmos três slots nativos ociosos, recalculados
todo frame por `Steerpoint::compute()` mas sem consumidor neste repositório.

## Verificação manual

```bash
python3 src/ui/scripts/edl_lint.py sandbox/A4-4DOF/configs/scenario_a4_4dof.edl.in

# edlcheck recusa o .edl.in cru por causa de @NUM_TC_THREADS@ -- resolva o token antes
sed 's/@NUM_TC_THREADS@/2/; s/@RUN_ID@/x/g' sandbox/A4-4DOF/configs/scenario_a4_4dof.edl.in > /tmp/a4-4.edl
./build/app/src/edlcheck /tmp/a4-4.edl

./build/app/src/app -folder ./sandbox -scenario A4-4DOF -deterministic 600 > /tmp/a4-4dof.log 2>&1
grep -o 'bt=[A-Za-z_-]*' /tmp/a4-4dof.log | sort -u    # esperado: so bt=NAV
grep -o 'player=a4_[0-9]' /tmp/a4-4dof.log | sort -u   # esperado: as 8 aeronaves

./tests/determinism/check_determinism.sh ./build/app/src/app A4-4DOF 2000 '' \
    sandbox/A4-4DOF/configs/scenario_a4_4dof.edl.in
```

## O que herda sem mudança

Tudo de `A4-6DOF` exceto o `dynamicsModel:` — os 53 componentes em cada uma
das oito aeronaves, a rota em figura-de-oito de 20 steerpoints com as três
`Action` (decoy/SAR/troca de camuflagem) disparando a cada volta,
`pilot:`/`Autopilot`, `agent:`/`flight_tree_nav.xml`, terreno SRTM
compartilhado, `provides:`. Ver `sandbox/A4-6DOF/README.md` e
`src/poc/built-in_mixr_1/README.md`.
