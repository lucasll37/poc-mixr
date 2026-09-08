# A4-3DOF — o player máximo, dinâmica RacModel (cinemática), navegando de verdade

Um dos **cinco** cenários da família `A4-*DOF` deste `sandbox/` — ver
`sandbox/A4-6DOF/README.md` para a tabela completa da família e a porta
Tacview compartilhada (**1234**, de propósito). Esta variante troca **só**
o `dynamicsModel:` em relação a `A4-6DOF`: tudo o resto (player, rota,
`agent:`/árvore) é byte a byte idêntico.

```bash
./build/app/src/app -folder ./sandbox -scenario A4-3DOF        # tempo real, Tacview 1234
./build/app/src/app -folder ./sandbox -scenario A4-3DOF -deterministic 200
```

## "3DOF" é um rótulo informal — leia isto antes de "corrigir" o nome

`RacModel` ("Robot Aircraft") **não tem doc string de graus de liberdade
nenhuma** no próprio header
(`contexts/src/mixr/include/mixr/models/dynamics/RacModel.hpp`: só "Very
simple dynamics model") — ao contrário de `LaeroModel` (`A4-4DOF`), que É
documentado como "4 degree of freedom" pelo próprio framework. `RacModel` é
melhor descrito como um modelo **cinemático**: rumo/altitude/velocidade são
cada um um canal comandado (`cmdHeading`/`cmdAltitude`/`cmdVelocity`) que
converge por um limitador de taxa simples, **sem estado de dinâmica
rotacional nenhum** (confirmado lendo `RacModel.cpp::updateRAC()` — não há
equação de momento resolvendo taxa de rolagem/arfagem/guinada a partir de
forças). "3DOF" aqui é o número de canais comandados independentes (rumo,
altitude, velocidade), não uma contagem formal do framework — decisão
tomada com o usuário ao criar este cenário, preferida a "1DOF" (que
sugeriria quase nenhuma liberdade, impreciso na outra direção).

## O bloco `dynamicsModel:`

```
dynamicsModel: ( RacModel
   minSpeed:    150.0
   speedMaxG:   400.0
   maxg:        4.0
   maxAccel:    10.0
   cmdAltitude: ( Meters 1750.0 )
   cmdHeading:  ( Degrees 90 )
   cmdSpeed:    350.0
)
```

Justificativa de cada número (sem precedente de uso de `RacModel` neste
repositório — nenhum `.edl` de produção o usa):

- `cmdAltitude`/`cmdHeading`/`cmdSpeed` = os mesmos `initAlt`/`initHeading`/
  `initVelocity` do player (1750 m / 90° / 350 kt) — estado inicial coerente
  com a pose real de partida; o `Autopilot` sobrescreve isso a cada frame de
  qualquer forma (`headingHoldMode`/`altitudeHoldMode`/`velocityHoldMode`
  são todos `true`).
- `maxg: 4.0` / `maxAccel: 10.0` = os próprios **defaults compilados** da
  classe (`RacModel.hpp`: `gMax{4.0}`, `maxAccel{10.0}`) — usa-se o default
  do autor em vez de inventar número novo.
- `minSpeed`/`speedMaxG` = escolhidos para manter a rampa de G graduada
  (não já no teto) na faixa de 350-370 kt que os steerpoints deste cenário
  comandam.

## Duas armadilhas do framework, não desta mudança

Documentadas aqui para ninguém "consertar" os números acima sem saber o motivo:

1. **Unidade inconsistente no `RacModel` nativo**: `RacModel::
   setSlotMinSpeed`/`setSlotSpeedMaxG` guardam o valor cru **em m/s** dentro
   de `updateRAC()`, apesar do comentário do slot dizer "(kts)" — confirmado
   lendo o fonte. Inconsistência nativa do MIXR, fora do escopo desta poc
   corrigir (o projeto trata o MIXR como dependência binária).
2. **Parâmetros secundários do `Autopilot` ficam inertes sob `RacModel`**:
   `maxRateOfTurnDps`/`maxBankAngle`/`maxPitchAngle`/`maxClimbRateMps`
   (copiados sem mudança no `pilot:`, herdados de `A4-6DOF`) são
   **ignorados por completo** pelos três `setCommanded*()` do `RacModel` —
   ao contrário do `LaeroModel` (`A4-4DOF`), que os usa de verdade.
   `RacModel` impõe seu próprio teto de taxa de giro/subida a partir de
   `minSpeed`/`speedMaxG`/`maxg`/`maxAccel`.

## Medido rodando (`-deterministic 200`)

`bt=NAV` em 100% das linhas — a rota de 4 steerpoints é voada de verdade
(igual a `A4-6DOF`/`A4-4DOF`, ao contrário de `A4-4DOF-PY`/`A4-4DOF-ONNX`).
`fuel=0.000000000`/`mach=0.000000000` no dump são esperados sob `RacModel`
(mesma explicação de `A4-4DOF/README.md` — `getFuelWt()` cru fica em 0 sem
tanque JSBSim simulado, mas `domain::WorldView::fuelFraction` cai para
`1.0`, não `0.0`, então `FuelLow` nunca dispara por engano).

## Verificação manual

```bash
python3 src/ui/scripts/edl_lint.py sandbox/A4-3DOF/configs/scenario_a4_3dof.edl.in
dist/bin/edlcheck sandbox/A4-3DOF/configs/scenario_a4_3dof.edl.in   # ver a nota do @NUM_TC_THREADS@ no README de A4-6DOF
./build/app/src/app -folder ./sandbox -scenario A4-3DOF -deterministic 200 > /tmp/a4-3dof.log 2>&1
grep -o 'bt=[A-Za-z_-]*' /tmp/a4-3dof.log | sort -u   # esperado: so bt=NAV
./tests/determinism/check_determinism.sh ./build/app/src/app A4-3DOF 2000 '' \
    sandbox/A4-3DOF/configs/scenario_a4_3dof.edl.in
```

Vale rodar por mais tempo (`-deterministic 3000`+) observando `alt=`/`spd=`
via `MsgReport name: telemetria` para confirmar que o `RacModel` converge
para cada altitude/velocidade comandada pelos steerpoints sem oscilar — o
mesmo tipo de validação que `full-systems-nav` fez para o `JSBSimModel`.

## O que herda sem mudança

Tudo de `A4-6DOF` exceto o `dynamicsModel:` — os 53 componentes, a rota de 4
steerpoints com as 4 `Action` disparando a cada volta, `pilot:`/`Autopilot`
(embora parte dele fique inerte, ver acima), `agent:`/`flight_tree_nav.xml`,
terreno, `provides:`.
