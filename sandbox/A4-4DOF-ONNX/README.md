# A4-4DOF-ONNX — o player máximo, dinâmica LaeroModel (4-DOF), decisão por rede ONNX

Um dos **cinco** cenários da família `A4-*DOF` deste `sandbox/` — ver
`sandbox/A4-6DOF/README.md` para a tabela completa da família e a porta
Tacview compartilhada (**1234**, de propósito). Duas mudanças em relação a
`A4-4DOF`: `dynamicsModel:` continua `( LaeroModel )`, mas quem decide o
rumo/altitude/velocidade deixa de ser o nó nativo `( Navigate )` — vira uma
rede neural (`( OnnxPolicy )`, 6.211 parâmetros, 25 KB), copiada e adaptada
de `src/poc/onnx-policy`.

```bash
./build/app/src/app -folder ./sandbox -scenario A4-4DOF-ONNX        # tempo real, Tacview 1234
./build/app/src/app -folder ./sandbox -scenario A4-4DOF-ONNX -deterministic 200
```

## Limitação conhecida, não um defeito — leia antes de achar que está quebrado

**Esta variante NUNCA completa a rota de 4 steerpoints, e as 4 `Action`
(decoy/SAR/troca de camuflagem/liberação de arma) NUNCA disparam** — mesma
causa raiz de `A4-4DOF-PY` (ver aquele README para o detalhe completo): o
contrato fixo de 28 floats que `OnnxPolicy` recebe é o **mesmo**
`libs/xrlbridge/ObservationFields.hpp` que `PyDecide` usa, e não carrega
rumo/altitude/velocidade do próximo steerpoint. Sem intruso neste cenário
hermético, a rede fica indefinidamente no regime de patrulha da barreira
para a qual foi treinada (`bt=ONNX` o tempo todo) — a rota nunca é voada de
verdade. `nav:`/`Route`/os 4 `Steerpoint` continuam declarados sem mudança
(herdados de `A4-4DOF`), mas nunca são alcançados fisicamente.

## A árvore: `Fallback` com uma rede (`./configs/flight_tree_onnx.xml`)

Cópia de `src/poc/onnx-policy/configs/flight_tree_onnx.xml`, com o caminho
de `model=` repontado:

```xml
<Fallback name="root">
  <OnnxPolicy model="./sandbox/A4-4DOF-ONNX/configs/policy_barrier.onnx"
              normalized="true" label="ONNX"/>
  <Patrol/>
</Fallback>
```

`./configs/policy_barrier.onnx` é uma cópia binária **verbatim** (25.775
bytes, `cmp` confirma idêntico) de `src/poc/onnx-policy/configs/
policy_barrier.onnx` — a mesma rede treinada por clonagem de comportamento
de uma barreira geométrica (`tools/train_policy.py`), sem RL. `( Patrol )`
só assume se a rede FALHAR ao carregar/inferir — os números que ele usa
(`patrolHeading`/`legTime`/`patrolAltitude`/`patrolSpeed`, e o resto da
tuning: `rtbAltitude`/`rtbSpeed`/`arrivalRadius`/`fuelReserve`/`breakTurn`/
`evadeClimb`/`evadeSpeed`/`supportSpeed`/`evadeHold`/`terrainClearance`)
vêm do `agent:`/`behavior:` no `.edl.in`, copiados de `src/poc/onnx-policy`
(falcon1) — a rede em si não lê nenhum deles.

**Também herdado de `onnx-policy`**: este cenário **não tem piso anti-CFIT
independente** (sem `( UbfArbiter )`/`( AltitudeSafetyBehavior )`, só
`( BtBehavior )` direto) — enquanto o `.onnx` carrega e infere com sucesso,
a rede tem a última palavra sobre altitude.

## Achado medindo: `fuel=0.000000000` no dump, sem efeito na árvore

Rodando `-deterministic 200`: `fuel=0.000000000`/`mach=0.000000000` no dump
(esperado sob `LaeroModel` — ver `A4-4DOF/README.md`), mas irrelevante aqui:
esta árvore nem tem condição `FuelLow` (só `OnnxPolicy`/`Patrol`), e a rede
não recebe combustível cru — o campo `fuelFraction` do contrato de 28 floats
(populado com a mesma guarda `fuelMax > 0.0 ? .. : 1.0` de `FlightState.cpp`)
é o único caminho, e cai para `1.0` sem tanque JSBSim simulado.

## Verificação manual

```bash
python3 src/ui/scripts/edl_lint.py sandbox/A4-4DOF-ONNX/configs/scenario_a4_4dof_onnx.edl.in
dist/bin/edlcheck sandbox/A4-4DOF-ONNX/configs/scenario_a4_4dof_onnx.edl.in   # ver a nota do @NUM_TC_THREADS@ no README de A4-6DOF
xmllint --noout sandbox/A4-4DOF-ONNX/configs/flight_tree_onnx.xml
./build/app/src/app -folder ./sandbox -scenario A4-4DOF-ONNX -deterministic 200 > /tmp/a4-4dof-onnx.log 2>&1
grep -o 'bt=[A-Za-z_-]*' /tmp/a4-4dof-onnx.log | sort -u   # esperado: so bt=ONNX (nunca bt=PATROL, que indicaria falha ao carregar o .onnx)
```

Sem cobertura de determinismo automatizada nesta variante (nenhum cenário
da família `A4-*DOF` entra em `tests/meson.build` — ver o README de
`A4-6DOF`), mas o mesmo script serve, se quiser confirmar:
```bash
./tests/determinism/check_determinism.sh ./build/app/src/app A4-4DOF-ONNX 2000 '' \
    sandbox/A4-4DOF-ONNX/configs/scenario_a4_4dof_onnx.edl.in
```

Se o `.onnx` for regenerado/retreinado: confirmar `ir_version <= 9` antes de
rodar (`tools/train_policy.py` de `onnx-policy` fixa isso; o pacote Python
`onnx` recente grava 13 por padrão, e o ONNX Runtime deste repo recusa
silenciosamente — sintoma: `bt=PATROL` em vez de `bt=ONNX`).

## O que herda sem mudança

`dynamicsModel: ( LaeroModel )` (igual a `A4-4DOF`), os 53 componentes do
player, `nav:`/`Route`/4 `Steerpoint` (estruturalmente presentes, nunca
voados), terreno, `provides:` do `PluginModule`.
