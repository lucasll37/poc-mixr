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

Nasceu como `sandbox/full-systems-nav` (ver `src/poc/full-systems-nav` para a
poc de referência completa), recolocado nesta família de comparação com nome e
porta consistentes com os outros 4. A **montagem do player** e a **pilha de
decisão** continuam intocadas — `dynamicsModel:` é `( JSBSimModel )` e o
`agent:` aponta para o `flight_tree_nav.xml` instalado (`( Navigate )`, um nó
só, sem Patrol/RTB/Evade por baixo). O que mudou desde então é a **frota**
(oito aeronaves empilhadas, não uma) e a **rota** (figura-de-oito de 20
steerpoints, não o circuito de 4 pontos).

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

## A rota — figura-de-oito de 20 steerpoints

Oito fechado, com **10 curvas para a direita seguidas de 10 para a esquerda** —
a rota não curva sempre para o mesmo lado. A construção:

```
rumo das cordas do lobo LESTE:  t + 18 + 36k   (k = 0..9)
rumo das cordas do lobo OESTE:  t - 18 - 36k   (k = 0..9)
```

As dez direções de um decágono somam **zero**, então cada lobo fecha sozinho no
ponto de cruzamento — o erro de fechamento do polígono é de **0,000 m**, não um
valor "pequeno o bastante". E nas duas junções o rumo é *contínuo* (o lobo leste
termina em `t−18` e o oeste começa em `t−18`; o oeste termina em `t+18` e o leste
começa em `t+18`): a aeronave **atravessa o cruzamento em linha reta**, como num
oito de verdade.

> **Armadilha da construção ingênua**, batida antes de chegar nesta: fazer os
> dois lobos partirem do cruzamento com o **mesmo** rumo também fecha e também
> alterna o lado da curva — mas aí os dois compartilham a perna inteira que sai
> dali (`wp1` e `wp11` caem no mesmo ponto), e o resultado não é um oito: são
> duas voltas sobre o mesmo trecho. O deslocamento de meia-perna (o `+18/−18`) é
> o que separa os dois casos.

`wp10` e `wp20` são o **mesmo ponto no mapa** (o cruzamento). É proposital: a
aeronave passa por ali duas vezes por volta, em alturas diferentes do perfil.

### Onde o oito foi posto — escolhido por varredura de terreno

207 colocações cabem inteiras no tile SRTM (`S23W043`); foram pontuadas pelo
relevo mais alto sob as 20 pernas. A vencedora põe o cruzamento em `(-24000, 0)` m
com `t = 36°`. O ganho não é só folga — os dois lobos caem sobre relevos
**muito** diferentes, e é isso que dá sentido ao perfil:

| lobo | steerpoints | relevo sob as pernas |
|---|---|---|
| **leste** | `wp1`..`wp9` | 5 a 250 m — litoral e mar, praticamente plano |
| **oeste** | `wp11`..`wp19` | 300 a 1.338 m — a serra |

Por isso a aeronave desce ao piso do perfil no lobo leste e só pode descer até
8.000 ft no lobo oeste. O perfil não foi colado por cima da geografia: saiu dela.

### Perfil de altitude (`a4_1`, a mais baixa), em pés

| wp | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 |
|---|--:|--:|--:|--:|--:|--:|--:|--:|--:|--:|
| alt | 11.000 | 8.000 | 6.000 | **4.000** | **4.000** | 6.000 | 9.000 | 12.000 | 14.000 | **15.000** |

| wp | 11 | 12 | 13 | 14 | 15 | 16 | 17 | 18 | 19 | 20 |
|---|--:|--:|--:|--:|--:|--:|--:|--:|--:|--:|
| alt | 15.000 | 13.000 | 11.000 | 9.000 | **8.000** | 10.000 | 12.000 | 14.000 | 15.000 | 13.000 |

São **duas descidas-e-subidas completas por volta**, uma por lobo: 11.000 →
4.000 → 15.000 ft no lobo leste (11.000 ft de amplitude) e 15.000 → 8.000 →
15.000 ft no oeste (7.000 ft). Ações: decoy em `wp5`, SAR em `wp9`, troca de
camuflagem em `wp15`.

**Por que o degrau máximo é de 3.000 ft por perna, e não mais** — medido, não
escolhido por gosto. Uma versão anterior deste perfil tinha dente-de-serra de
±4.000 ft com período de **uma** perna (~110 s) no lobo alto; rodando, a malha de
altitude não acompanha: a aeronave achatava em torno de 12.500 ft (`a4_1`) /
18.900 ft (`a4_8`) e ficava **2.200 a 3.100 ft atrás** do comando ponto a ponto —
ou seja, o perfil "mais acentuado" no papel virava um perfil **menos** variado no
ar. Com as rampas espalhadas por várias pernas o erro na passagem cai para a casa
das centenas de pés. Amplitude total preservada; o que mudou foi a taxa.

### A pilha de oito aeronaves

`a4_1`..`a4_8` são idênticas (mesmo player máximo, mesmo `agent:`, mesma árvore) e
cumprem a mesma rota; a única diferença é **+1.000 ft por aeronave em todos os 20
steerpoints**. Nascem na mesma posição horizontal, empilhadas.

- **O `airspeed:` de cada steerpoint é recalculado por aeronave**, e é isso que
  impede a pilha de se desmanchar. A malha de velocidade do `a4ap.xml` fecha
  contra `velocities/vc-kts` (velocidade **calibrada**), enquanto quem define a
  geometria da curva é a **verdadeira**. Comandar a mesma calibrada nas oito faria
  a `a4_8`, 7.000 ft acima, voar ~13% mais rápido em verdadeira e escapar da
  formação em uma volta. Cada steerpoint carrega a calibrada que dá **262 kt
  verdadeiros (135 m/s)** na altitude *daquele* ponto.
- `( CollisionDetect )` tem `collisionRange: 100 m` e `sendCrashEvents: false`.
  1.000 ft = 305 m, então a pilha nem entra no alcance de detecção — e mesmo que
  entrasse, não há `CRASH_EVENT` vindo dali.

### Por que pernas de 14 km (lobos de 22,7 km de raio)

Medido rodando, e é o mesmo limite que já dimensionava a rota anterior.
`maxBankAngle`/`maxRateOfTurnDps` do `( Autopilot )` são **inertes** com
`( JSBSimModel )` — os dois chegam a `JSBSimModel::setCommandedHeadingD(h,,)` como
parâmetros **sem nome** e são descartados. Quem manda é o autopiloto do próprio
airframe, `models/players/A-4/data/jsbsim/aircraft/A4/a4ap.xml`: erro de rumo
limitado a ±30°, ganho 1:1 de erro para inclinação, e um nivelador de asa
(`−0.8·phi`) sempre ligado que na prática segura o banco em ~25°. A 135 m/s isso
dá raio de curva de ~4,8 km e uma **reversão de banco de ~30 s**. Em circuito
apertado a aeronave passa do rumo da perna seguinte em 40–100°, oscila com período
de ~200 s e termina orbitando um steerpoint que caiu dentro do próprio círculo de
curva — para de sequenciar e nunca fecha a volta. Aqui a perna é ~2,9 vezes o raio
de curva, a mesma folga já medida como suficiente.

`autoSeqDistance: 2.0 NM` (3,7 km) é a antecipação de curva — o dobro do
`r·tan(18°) = 1,6 km` que a geometria pediria, dobrado para cobrir o atraso de
rolagem. A aeronave corta o canto de cada vértice: comportamento normal de ponto
de passagem (*fly-by*), não de sobrevoo.

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
`enum StptType { DEST, MARK, FIX, OAP, IP, TGT, TGT_GRP }`), repetidos ao longo
dos 20 pontos. O sétimo, **`TGT_GRP`, não é alcançável por
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
grep -o 'player=a4_[0-9]' /tmp/a4-6dof.log | sort -u  # esperado: as 8 aeronaves

# uma volta inteira (~2100 s simulados) -- e o que exercita a rota de verdade;
# 200 frames sequer saem da primeira perna. Leva ~3 min de relogio de parede.
./build/app/src/app -folder ./sandbox -scenario A4-6DOF -deterministic 118000 > /tmp/a4-volta.log 2>&1

# NENHUMA aeronave pode raspar o solo: o menor AGL de todas, em metros
awk -F'agl=' '/^frame=/{split($2,a," ");print a[1]}' /tmp/a4-volta.log | sort -g | head -1

# faixa de altitude da mais baixa e da mais alta (pes)
for p in a4_1 a4_8; do
  awk -v p="player=$p " -F'alt=' '$0 ~ p && /^frame=/{split($2,a," ");printf "%.0f\n",a[1]*3.28084}' \
    /tmp/a4-volta.log | sort -n | sed -n "1p;\$p" | paste -sd' ' - | sed "s/^/$p: /"
done

./tests/determinism/check_determinism.sh ./build/app/src/app A4-6DOF 2000 '' \
    sandbox/A4-6DOF/configs/scenario_a4_6dof.edl.in    # determinismo com 1, 2 e 4 threads T/C
```

`edlcheck` recusa o `.edl.in` cru com `error while setting slot name:
numTcThreads` — o token `@NUM_TC_THREADS@` só é expandido em runtime por
`app::generateScenario()` (`ScenarioTemplate`), nunca pelo `edlcheck`. Mesmo
comportamento do `full-systems-nav` original; para validar via `edlcheck`
direto, resolva o token manualmente antes (`sed 's/@NUM_TC_THREADS@/2/'`).

## O que herda sem mudança

Os 53 componentes nativos (em **cada uma** das oito aeronaves), as 3 `Action`
restantes (decoy/SAR/troca de camuflagem, disparando A CADA VOLTA — `wrap: true`,
em `wp5`/`wp9`/`wp15`), terreno SRTM compartilhado, `provides:` do `PluginModule`
(as mesmas 9 entradas) — ver `sandbox/full-systems-nav`'s histórico (agora
removido deste `sandbox/`, mas com a poc de referência intacta em
`src/poc/full-systems-nav`) e `src/poc/built-in_mixr_1/README.md` para a
tabela completa dos 53 componentes e as 8 armadilhas de montagem
(`Table2`/`LatLon`/`dataLogTime:` etc.).
