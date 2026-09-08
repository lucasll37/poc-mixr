# A4-4DOF-PY — o player máximo, dinâmica LaeroModel (4-DOF), decisão em Python

Um dos **cinco** cenários da família `A4-*DOF` deste `sandbox/` — ver
`sandbox/A4-6DOF/README.md` para a tabela completa da família e a porta
Tacview compartilhada (**1234**, de propósito). Duas mudanças em relação a
`A4-4DOF`: `dynamicsModel:` continua `( LaeroModel )`, mas quem decide o
rumo/altitude/velocidade deixa de ser o nó nativo `( Navigate )` — vira uma
árvore com folhas em Python (`( PyDecide )`), copiada e adaptada de
`src/poc/python-flight`.

```bash
./build/app/src/app -folder ./sandbox -scenario A4-4DOF-PY        # tempo real, Tacview 1234
./build/app/src/app -folder ./sandbox -scenario A4-4DOF-PY -deterministic 200
```

## Limitação conhecida, não um defeito — leia antes de achar que está quebrado

**Esta variante NUNCA completa a rota de 4 steerpoints, e as 4 `Action`
(decoy/SAR/troca de camuflagem/liberação de arma) NUNCA disparam.** Isso é
esperado, não um bug de implementação:

O contrato fixo de 28 floats que `PyDecide` recebe
(`libs/xrlbridge/ObservationFields.hpp`) só carrega cinemática do próprio
avião + contato de radar + alerta + combustível (o vocabulário de
patrulha/evasão/apoio/RTB da produção) — **nunca** rumo/altitude/velocidade
do próximo steerpoint. Esses campos (`navTrueBrgDeg`/`navCmdAltM`/
`navCmdSpeedKts`) **existem** em `domain::WorldView` (é o que `( Navigate )`
lê nos outros 3 cenários da família), mas nunca são expostos no contrato de
28 floats — confirmado lendo os dois arquivos. Sem esse dado, um script
Python não tem como saber "vire para o waypoint 2 agora".

Sem intruso/datalink neste cenário hermético (um player só, sem
`networks:`), o resultado prático é que o avião fica **indefinidamente em
`PY-PATROL`** — a mesma órbita geométrica de `policy/patrol.py` — e nunca
sai desse regime. `nav:`/`Route`/os 4 `Steerpoint` continuam declarados sem
mudança (herdados de `A4-4DOF`), mas nunca são alcançados fisicamente.

## A árvore: `Fallback` com folhas em Python (`./configs/flight_tree_python.xml`)

Mesma forma de `src/poc/python-flight/configs/flight_tree_python.xml`, com
os caminhos de `script=` repontados para esta pasta:

| prioridade | condição (C++) | ação (Python, editável) |
|---|---|---|
| 1) pouco combustível | `FuelLow margin="0.05"` | `./configs/policy/rtb.py` |
| 2) evasão valendo | `ContactDetected` | `./configs/policy/evade.py` |
| 3) alerta recebido | `AlertReceived` | `./configs/policy/support.py` |
| 4) nada acontecendo | — | `./configs/policy/patrol.py` |
| (degradação) | — | `( Patrol )` nativo, fim do `Fallback` |

Os 4 scripts foram copiados **verbatim** de `src/poc/python-flight/configs/
policy/` (não têm caminho embutido — só indexam a lista de 28 floats por
constante). `agent:`/`behavior:` no `.edl.in` ganhou as slots de tuning
`patrolHeading`/`legTime`/`patrolAltitude`/`patrolSpeed`/`rtbAltitude`/
`rtbSpeed`/`arrivalRadius`/`fuelReserve`/`breakTurn`/`evadeClimb`/
`evadeSpeed`/`supportSpeed`/`evadeHold`/`terrainClearance` (mesmos valores
de `src/poc/python-flight`, falcon1) — o nó nativo `( Navigate )` não
precisa delas, mas o `( Patrol )`/`( ReportAndEvade )` usados como
degradação dentro da árvore Python precisam.

## Achado medindo: `fuel=0.000000000` no dump não engana o `FuelLow`

Rodando `-deterministic 200`, o dump mostra `fuel=0.000000000` (esperado
sob `LaeroModel` — ver `A4-4DOF/README.md`). Isso poderia sugerir, à
primeira vista, que `FuelLow margin="0.05"` dispararia sempre (combustível
sempre "zero"). **Medido rodando: não dispara** — `bt=PY-PATROL` em 100%
das linhas, nunca `PY-RTB`. Confirmado lendo `FlightState.cpp`:
`domain::WorldView::fuelFraction` usa uma guarda explícita —
`(fuelMax > 0.0) ? (getFuelWt()/fuelMax) : 1.0` — então sem tanque JSBSim
simulado ele cai para `1.0` (tanque cheio), não `0.0`. O campo `fuel=` do
dump é um valor cru diferente (`getFuelWt()`), não o que a árvore consulta.

## Vocabulário extra do Steerpoint: `sca`/`magvar`/`pta`

Os 4 `Steerpoint` de `nav:`/`Route` (herdados sem mudança, ver acima) ganharam os
mesmos três slots nativos ociosos que a família `A4-6DOF`/`A4-4DOF`/`A4-3DOF`
ganhou — `sca`/`magvar`/`pta` (ver `sandbox/A4-6DOF/README.md` para o detalhe
completo). Como esta rota nunca é alcançada por `( PyDecide )`, os três campos
continuam tão inertes quanto o resto do `nav:` — mantidos aqui só por
consistência de vocabulário com o resto da família.

## Verificação manual

```bash
python3 src/ui/scripts/edl_lint.py sandbox/A4-4DOF-PY/configs/scenario_a4_4dof_py.edl.in
dist/bin/edlcheck sandbox/A4-4DOF-PY/configs/scenario_a4_4dof_py.edl.in   # ver a nota do @NUM_TC_THREADS@ no README de A4-6DOF
xmllint --noout sandbox/A4-4DOF-PY/configs/flight_tree_python.xml
./build/app/src/app -folder ./sandbox -scenario A4-4DOF-PY -deterministic 200 > /tmp/a4-4dof-py.log 2>&1
grep -o 'bt=[A-Za-z_-]*' /tmp/a4-4dof-py.log | sort -u   # esperado: so bt=PY-PATROL (nunca EVADE/SUPPORT/RTB)
```

Sem cobertura de determinismo automatizada nesta variante (nenhum cenário
da família `A4-*DOF` entra em `tests/meson.build` — ver o README de
`A4-6DOF`), mas o mesmo script serve, se quiser confirmar:
```bash
./tests/determinism/check_determinism.sh ./build/app/src/app A4-4DOF-PY 2000 '' \
    sandbox/A4-4DOF-PY/configs/scenario_a4_4dof_py.edl.in
```

## O que herda sem mudança

`dynamicsModel: ( LaeroModel )` (igual a `A4-4DOF`), os 53 componentes do
player, `nav:`/`Route`/4 `Steerpoint` (estruturalmente presentes, nunca
voados), terreno, `provides:` do `PluginModule`.
