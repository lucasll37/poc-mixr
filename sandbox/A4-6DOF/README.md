# A4-6DOF — oito players máximos empilhados, dinâmica JSBSim (6-DOF), voando uma figura-de-oito

Um dos **cinco** cenários da família `A4-*DOF` deste `sandbox/` — todos o mesmo
player `a4` (os 53 componentes nativos do "player máximo", ver
`src/poc/built-in_mixr_1`), a mesma porta Tacview (**1234**, de propósito —
nenhum dos 5 roda ao mesmo tempo que outro, então reusar a porta padrão evita
decorar 5 números diferentes). Entre os cinco variam dois eixos: o
`dynamicsModel:` e quem decide o rumo/altitude/velocidade.

> **Este cenário divergiu dos outros quatro em duas coisas.** (1) A **frota**:
> são **oito** aeronaves idênticas, `a4_1`..`a4_8`, cumprindo a mesma rota com
> **1.000 ft de separação vertical** entre uma e a seguinte — uma pilha de
> 7.000 ft de altura, não um player solitário. (2) A **rota**: uma
> **figura-de-oito fechada de 20 steerpoints**, com perfil de altitude entre
> **4.000 e 15.000 ft** para a aeronave mais baixa (11.000 a 22.000 ft para a
> mais alta). Os outros quatro continuam com um player e a rota original de 4
> steerpoints num circuito de ~3 km de raio, e os READMEs deles ainda afirmam
> que "tudo o resto é byte a byte idêntico a `A4-6DOF`" — o que continua valendo
> para a montagem do player, o `agent:`/árvore e o resto do cenário, mas **não**
> para a frota nem para a rota.
>
> **Não há lançamento de bomba nem de míssil.** A `ActionWeaponRelease` que
> existia num steerpoint foi removida — era o único ponto do cenário que soltava
> algo letal. As estações do `( StoresMgr )` continuam declaradas (fazem parte
> das 53 classes nativas que este cenário existe para montar), mas nada as
> libera. Restam três `Action`: decoy, SAR e troca de camuflagem.

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

## A rota — 10 steerpoints, 5.000 → 15.000 → 5.000 ft

Decágono regular de **24 km de raio** centrado em `(2000, 0)` m do referencial da
`Station`: perna de 14,8 km, 36° de curva por vértice, volta nominal de ~1.030 s
(~17 min) de tempo simulado. A altitude comandada sobe em degraus de `wp1`
(5.000 ft) até `wp6` (15.000 ft) e desce de volta:

| wp | tipo | alt (ft) | CAS (kt) | ação |
|---:|------|---------:|---------:|------|
| 1  | DEST | 5.000  | 260 | `ActionDecoyRelease` |
| 2  | FIX  | 6.500  | 255 | — |
| 3  | TGT  | 8.500  | 245 | `ActionImagingSar` |
| 4  | MARK | 11.000 | 235 | — |
| 5  | OAP  | 13.500 | 230 | — |
| 6  | FIX  | 15.000 | 220 | `ActionCamouflageType` |
| 7  | MARK | 13.000 | 230 | — |
| 8  | IP   | 10.500 | 240 | `ActionWeaponRelease` |
| 9  | OAP  | 8.000  | 250 | — |
| 10 | FIX  | 6.000  | 255 | — |

**Por que o circuito é tão grande** — medido rodando, não estimado.
`maxBankAngle`/`maxRateOfTurnDps` do `( Autopilot )` são **inertes** com
`( JSBSimModel )` (os dois chegam a `JSBSimModel::setCommandedHeadingD()` como
parâmetros sem nome e são descartados). Quem manda é o autopiloto do próprio
airframe, `models/players/A-4/data/jsbsim/aircraft/A4/a4ap.xml`: erro de rumo
limitado a ±30°, ganho 1:1 para inclinação, e um nivelador de asa sempre ligado
que segura o banco em ~25°. A ~145 m/s isso dá **1,7 °/s** de taxa de curva
(raio ~5,5 km) e uma **reversão de banco de ~30 s**. Em circuitos menores a
aeronave passa do rumo da perna seguinte em 40–100°, oscila com período de
~200 s e acaba orbitando um steerpoint que caiu dentro do próprio círculo de
curva — para de sequenciar e nunca fecha a volta:

| raio | resultado medido |
|---:|---|
| 12 km | sequenciou 8 dos 10 pontos e travou |
| 20 km | fecha volta, com pernas de 230 s onde a nominal é 86 s |
| **24 km** | **2,6 voltas em 3.400 s, os 10 pontos na ordem em toda volta** |
| 32 km | idem, com folga — ao custo de 23 min/volta |

**Isso não é regressão deste perfil**: a rota anterior, de 4 steerpoints num
circuito de ~3 km, também nunca sequenciava — a aeronave entrava em órbita
permanente em torno de `wp2`. O critério que este README afirmava (só `bt=NAV`)
continuava verdadeiro o tempo todo, porque `( Navigate )` segue decidindo mesmo
perseguindo um ponto que nunca alcança.

**As velocidades são um escalonamento de subida**, não números soltos: a malha
de velocidade do `a4ap.xml` fecha contra `velocities/vc-kts` (calibrada),
enquanto o raio de curva depende da verdadeira. Caindo de 260 para 220 kt
calibrados, a verdadeira fica em ~145 m/s do piso ao teto e a geometria do
circuito vale igual nas dez pernas.

**Folga de terreno**: as 10 posições e as 10 pernas foram conferidas contra o
tile SRTM real (`S23W043`, 81 amostras por perna). Relevo mais alto sob o
circuito: 1.588 m. Folga mínima entre a altitude comandada mais baixa de cada
perna e o relevo daquela perna: **633 m**. Medido na execução: AGL mínimo de
581 m.

**Medido em 3.400 s (2,6 voltas)**: os 10 steerpoints sequenciados na ordem em
toda volta; erro de altitude na passagem de **100 ft na mediana, 288 ft no pior
ponto**; passagem a ~3,6 km de cada vértice (corta o canto — comportamento
normal de *fly-by*, com `autoSeqDistance: 2.0 NM`); `bt=NAV` em 100% das linhas.

## Vocabulário extra do Steerpoint: `sca`/`magvar`/`pta`

Os `Steerpoint` da rota carregam três slots nativos que `mixr::models::Steerpoint`
já aceita (`Steerpoint.hpp`) e que a rota original não usava: `sca` (Safe Clearance
Altitude, pés), `magvar` (declinação magnética, graus) e `pta` (Planned Time of
Arrival, segundos). Os três são recalculados **todo frame** por
`Steerpoint::compute()` — mas **nenhum consumidor deste repositório**
(`FlightState.cpp`/`NavigateAction.cpp`/`libs/xmsg`) lê o resultado
(`isWarnSCA()`/`getMagBrgDeg()`/`getELT()`). A adição é por completude do
vocabulário EDL, não por efeito medido no voo — comentário detalhado no
`.edl.in`, junto de cada `wp*:`.

`stptType` agora cobre **6 dos 7** valores nativos (`Steerpoint.hpp`:
`enum StptType { DEST, MARK, FIX, OAP, IP, TGT, TGT_GRP }`) — com 10 pontos,
`MARK` e `OAP` passaram a caber. O sétimo, **`TGT_GRP`, não é alcançável por
EDL**: existe no enum, mas `Steerpoint::setSlotStptType()`
(`Steerpoint.cpp:325-340`) só reconhece as seis strings acima e recusa qualquer
outra com *"invalid steerpoint type"* — escrever `TGT_GRP` no `.edl` derruba o
slot, não seleciona o tipo. (A versão anterior deste README dizia que os três
não usados tinham ficado de fora "de propósito"; um deles simplesmente não é
construível.)

## Verificação manual

```bash
python3 src/ui/scripts/edl_lint.py sandbox/A4-6DOF/configs/scenario_a4_6dof.edl.in
dist/bin/edlcheck sandbox/A4-6DOF/configs/scenario_a4_6dof.edl.in   # ver nota abaixo sobre @NUM_TC_THREADS@
./build/app/src/app -folder ./sandbox -scenario A4-6DOF -deterministic 200 > /tmp/a4-6dof.log 2>&1
grep -o 'bt=[A-Za-z_-]*' /tmp/a4-6dof.log | sort -u   # esperado: so bt=NAV

# uma volta inteira (~1030 s simulados) -- e o que exercita a rota de verdade;
# 200 frames sequer saem da primeira perna
./build/app/src/app -folder ./sandbox -scenario A4-6DOF -deterministic 60000 > /tmp/a4-volta.log 2>&1
awk -F'alt=' '/^frame=/{split($2,a," ");printf "%.0f\n",a[1]*3.28084}' /tmp/a4-volta.log \
  | sort -n | sed -n '1p;$p'                          # esperado: ~4870 e ~15160 ft

./tests/determinism/check_determinism.sh ./build/app/src/app A4-6DOF 2000 '' \
    sandbox/A4-6DOF/configs/scenario_a4_6dof.edl.in    # determinismo com 1, 2 e 4 threads T/C
```

`edlcheck` recusa o `.edl.in` cru com `error while setting slot name:
numTcThreads` — o token `@NUM_TC_THREADS@` só é expandido em runtime por
`app::generateScenario()` (`ScenarioTemplate`), nunca pelo `edlcheck`. Mesmo
comportamento do `full-systems-nav` original; para validar via `edlcheck`
direto, resolva o token manualmente antes (`sed 's/@NUM_TC_THREADS@/2/'`).

## O que herda sem mudança

Os 53 componentes nativos, as 4 `Action` (decoy/SAR/troca de camuflagem/
liberação de arma, disparando A CADA VOLTA — `wrap: true`, agora distribuídas em
`wp1`/`wp3`/`wp6`/`wp8`), terreno SRTM compartilhado, `provides:` do `PluginModule` (as
mesmas 9 entradas) — ver `sandbox/full-systems-nav`'s histórico (agora
removido deste `sandbox/`, mas com a poc de referência intacta em
`src/poc/full-systems-nav`) e `src/poc/built-in_mixr_1/README.md` para a
tabela completa dos 53 componentes e as 8 armadilhas de montagem
(`Table2`/`LatLon`/`ActionWeaponRelease.station`/`dataLogTime:` etc.).
