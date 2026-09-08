# A4-6DOF — o player máximo, dinâmica JSBSim (6-DOF), navegando de verdade

Um dos **cinco** cenários da família `A4-*DOF` deste `sandbox/` — todos o mesmo
player `a4` (os 53 componentes nativos do "player máximo", ver
`src/poc/built-in_mixr_1`), a mesma rota de 4 steerpoints, a mesma porta
Tacview (**1234**, de propósito — nenhum dos 5 roda ao mesmo tempo que outro,
então reusar a porta padrão evita decorar 5 números diferentes). Só variam
dois eixos: o `dynamicsModel:` e quem decide o rumo/altitude/velocidade.

| pasta | `dynamicsModel:` | quem decide |
|---|---|---|
| **A4-6DOF** (esta) | `( JSBSimModel )` | `( Navigate )` nativo, voa a rota real |
| `A4-4DOF` | `( LaeroModel )` | `( Navigate )` nativo, voa a rota real |
| `A4-3DOF` | `( RacModel )` | `( Navigate )` nativo, voa a rota real |
| `A4-4DOF-PY` | `( LaeroModel )` | script Python (`PyDecide`) — patrulha só |
| `A4-4DOF-ONNX` | `( LaeroModel )` | rede ONNX (`OnnxPolicy`) — patrulha só |

```bash
./build/app/src/app -folder ./sandbox -scenario A4-6DOF        # tempo real, Tacview 1234
./build/app/src/app -folder ./sandbox -scenario A4-6DOF -deterministic 200
```

## O que é

É `sandbox/full-systems-nav` original (ver `src/poc/full-systems-nav` para a
poc de referência completa), recolocado nesta família de comparação com nome
e porta consistentes com os outros 4. **Nenhuma mudança funcional** —
`dynamicsModel:` continua `( JSBSimModel )` e o `agent:` continua apontando
para o `flight_tree_nav.xml` instalado (`( Navigate )`, um nó só, sem
Patrol/RTB/Evade por baixo).

## 6-DOF: a linha de base da família

`JSBSimModel` é o único dos três `dynamicsModel:` desta família com
**contagem de DOF documentada de verdade** pelo framework — `6DOF`, os 3
graus translacionais + 3 rotacionais que o FDM do JSBSim integra por completo
(`contexts/MIXR-CONTEXT.md`: "`JSBSimModel` — fidelidade completa (6DOF)").
`( Autopilot )` funciona sobre ele através do FCS interno do próprio
airframe JSBSim (`data/jsbsim/aircraft/A4/systems/`) — é por isso que os
outros 4 cenários da família, que trocam de `dynamicsModel:`, precisam
confirmar que `Autopilot::setCommandedHeadingD/Altitude/VelocityKts` ainda
tem efeito (ver os READMEs deles).

## Verificação manual

```bash
python3 src/ui/scripts/edl_lint.py sandbox/A4-6DOF/configs/scenario_a4_6dof.edl.in
dist/bin/edlcheck sandbox/A4-6DOF/configs/scenario_a4_6dof.edl.in   # ver nota abaixo sobre @NUM_TC_THREADS@
./build/app/src/app -folder ./sandbox -scenario A4-6DOF -deterministic 200 > /tmp/a4-6dof.log 2>&1
grep -o 'bt=[A-Za-z_-]*' /tmp/a4-6dof.log | sort -u   # esperado: so bt=NAV
./tests/determinism/check_determinism.sh ./build/app/src/app A4-6DOF 2000 '' \
    sandbox/A4-6DOF/configs/scenario_a4_6dof.edl.in    # determinismo com 1, 2 e 4 threads T/C
```

`edlcheck` recusa o `.edl.in` cru com `error while setting slot name:
numTcThreads` — o token `@NUM_TC_THREADS@` só é expandido em runtime por
`app::generateScenario()` (`ScenarioTemplate`), nunca pelo `edlcheck`. Mesmo
comportamento do `full-systems-nav` original; para validar via `edlcheck`
direto, resolva o token manualmente antes (`sed 's/@NUM_TC_THREADS@/2/'`).

## O que herda sem mudança

Os 53 componentes nativos, a rota de 4 steerpoints com as 4 `Action`
(decoy/SAR/troca de camuflagem/liberação de arma, disparando A CADA VOLTA —
`wrap: true`), terreno SRTM compartilhado, `provides:` do `PluginModule` (as
mesmas 9 entradas) — ver `sandbox/full-systems-nav`'s histórico (agora
removido deste `sandbox/`, mas com a poc de referência intacta em
`src/poc/full-systems-nav`) e `src/poc/built-in_mixr_1/README.md` para a
tabela completa dos 53 componentes e as 8 armadilhas de montagem
(`Table2`/`LatLon`/`ActionWeaponRelease.station`/`dataLogTime:` etc.).
