# A4-6DOF-RANDOM — as oito aeronaves de `A4-6DOF`, cada uma fazendo um *slow roll* de tempos em tempos

Mesmo cenário de [`A4-6DOF`](../A4-6DOF/) — oito `a4_1..a4_8` empilhadas com 1.000 ft de
separação, voando a figura-de-oito de 20 steerpoints com dinâmica JSBSim 6-DOF — com **duas**
diferenças:

1. o `treeFile:` aponta para `flight_tree_random.xml`, um `Fallback` de dois ramos —
   `( SlowRoll )` por cima de `( Navigate )` — em vez do `( Navigate )` solitário de
   `flight_tree_nav.xml`;
2. quatro slots novos do `( BtBehavior )` configuram a acrobacia, mais a `patrolMasterSeed`
   que semeia o sorteio.

```bash
./build/app/src/app -folder ./sandbox -scenario A4-6DOF-RANDOM        # tempo real, Tacview 1234
./build/app/src/app -folder ./sandbox -scenario A4-6DOF-RANDOM -deterministic 2000
```

## A pergunta que este cenário responde

> Dá para um nó de árvore de comportamento comandar uma **acrobacia** — rolagem em torno do
> eixo longitudinal — em instantes **aleatórios mas reprodutíveis**, sobre a mesma pilha
> nativa (JSBSim + `Autopilot` + `FlightAgentTC` na fase 3)?

Dá, e as duas metades da resposta custaram coisas diferentes.

### A aleatoriedade reprodutível

A semente do **cenário** é uma só (`patrolMasterSeed: 20260910`), repetida **idêntica** nas
oito aeronaves. Cada `BtBehavior` deriva dela a própria sub-semente a partir do **hash do
próprio nome** e, dessa, deriva de novo com um *salt* de propósito o gerador do intervalo
entre acrobacias:

```
patrolMasterSeed  --(fnv1a64("a4_3"))-->  instanceSeed  --(kSlowRollSalt)-->  gerador do a4_3
```

Nada vem de ordem de descoberta nem de índice na lista de players — e é isso que faz o
resultado ser idêntico com 1, 2 ou 4 threads de tempo crítico, apesar de as oito decidirem
**em paralelo**, uma por thread do pool. Ver [`libs/xrandom`](../../libs/xrandom/).

O intervalo é sorteado em `[slowRollMinInterval, slowRollMaxInterval]` **uma vez, no fim de
cada manobra** — nunca por `dt`. Esse é o mesmo invariante que `domain::PatrolPlan` já
documenta para o jitter de rumo: o número de manobras que um player executa não depende de
quantas threads existem; a cadência de `dt` por frame, não.

Efeito visível no Tacview: as oito rolam em instantes **diferentes entre si**, e duas
execuções do mesmo cenário rolam nos **mesmos** instantes.

### A manobra em si — o que não era óbvio

**Não existe comando de banco no `Autopilot` do MIXR.** Não há `setCommandedRollAngleD` nem
equivalente; `maxBankAngle` é *limite* repassado ao dynamics model, não *setpoint*. O único
caminho é o stick de aileron normalizado — e ele só alcança o dynamics model pelo ramo
`else` de `Autopilot::headingController()`, ou seja **com o heading hold desligado**. Por
isso o comando de rolagem viaja num campo próprio do `domain::FlightCommand`
(`rollOverride`/`rollStick`), e a atuação tem um ramo separado.

O comando vai **no `Autopilot`, nunca no `AirVehicle`**: `headingController()` roda toda
fase 3 e sobrescreve o dynamics model com o `stickRollPos` do próprio `Autopilot`, então um
`AirVehicle::setControlStick()` seria zerado no frame seguinte.

**E, mesmo com o stick cheio, a aeronave não fechava o giro.** O nivelador de asas de
`a4ap.xml` (`-0.8` sobre `attitude/phi-rad`) somava-se ao aileron do piloto e zerava o
comando líquido quando `0,8·φ + 0,6·p = 1` — 71,6° de banco parado, e apenas ~50° a
0,5 rad/s de taxa. A aeronave travava de lado e **nunca** fechava um tonneau, sem erro
nenhum. Esse nivelador passou a ser **gateado em `ap/heading_hold == 1`**, que o voo normal
mantém ligado o tempo todo (`FlightAction::execute()` o liga em toda decisão atuada) — então
nenhum cenário existente mudou, e a acrobacia ganhou a autoridade de que precisava.

O **amortecedor de taxa** (`-0,6·p`) continua sempre ativo, e é ele que limita a taxa de
rolagem durante a manobra: é ele que faz o giro ser *slow*.

## Os slots

| slot | valor aqui | o que faz |
|---|---|---|
| `slowRollMinInterval` | `( Seconds 45 )` | piso do intervalo entre acrobacias |
| `slowRollMaxInterval` | `( Seconds 120 )` | teto; `<=` ao piso vira intervalo **fixo**, sem sorteio |
| `slowRollStick` | `0.9` | aileron durante o giro, `-1..1`; o **sinal** é o sentido. **`0` desliga** (default) |
| `slowRollTimeout` | `( Seconds 20 )` | aborta a manobra que não fecha os 360° |
| `slowRollMinMargin` | `( Meters 1500 )` | folga sobre o piso anti-CFIT exigida para **começar** um giro; sem ela, o sorteio fica adiado (default do modelo: 1500 m; negativo desliga a borda) |
| `terrainClearance` | `( Meters 1200 )` | folga sobre o terreno que `( Navigate )` mantém **o tempo todo**, não só no instante do giro (default do modelo: 500 m — explicitamente elevado aqui, ver "Corrigido" abaixo) |
| `patrolMasterSeed` | `20260910` | a semente do **cenário** (o nome é histórico — é dela que todo gerador do player descende) |

`slowRollStick: 0` é o default, ou seja **o recurso nasce desligado**: um cenário que aponte
para `flight_tree_random.xml` sem declarar os slots voa exatamente como `flight_tree_nav.xml`.

`slowRollTimeout` não é luxo. Sem ele, uma aeronave sem autoridade de rolagem suficiente
ficaria presa em manobra **para sempre**, sem erro nenhum — exatamente o que aconteceria com
o nivelador de asas não gateado. Com ele, esse defeito vira um sintoma observável.

## Armadilhas

1. **O stick é pegajoso do lado do JSBSim.** `FCS` guarda o último `SetDaCmd()` e nada o
   reaplica por frame — sair da manobra sem zerar deixaria a aeronave rolando para sempre.
   `FlightAction::execute()` zera explicitamente no ramo normal, o que cobre toda saída da
   manobra, inclusive a que não passa pelo nó.
2. **`setNavMode(true)` religa os três hold modes de uma vez**, e `modeManager()` o chama toda
   fase 3. O ramo de rolagem desliga navMode explicitamente, mesmo os cenários declarando
   `navMode: false`.
3. **O banco lido vive em (-180, 180]** — `attitude/phi-rad` é um `atan2` sem wrap. O
   acumulador de giro integra a **diferença** entre ticks passada por `wrap180`; comparar o
   ângulo absoluto contra 360 nunca dispararia.
4. **`edlcheck` recusa o `.edl.in` cru** (`@NUM_TC_THREADS@` só é expandido em runtime).
   Para validar à mão:
   ```bash
   sed 's/@NUM_TC_THREADS@/2/' sandbox/A4-6DOF-RANDOM/configs/scenario_a4_6dof_random.edl.in > /tmp/a4rnd.edl
   ./dist/bin/edlcheck /tmp/a4rnd.edl
   ```

## O que foi medido rodando

| medida | valor |
|---|---|
| duração de um giro de 360° | **~7,9 s** (≈46 °/s médios, com `slowRollStick: 0.9`) — fecha por conclusão, não por timeout |
| banco máximo observado | **±177°** — passa folgadamente da barreira de 71,6° que existia antes do gate |
| custo em altitude por giro | **~530 m** (~1.700 ft), medido contra o mesmo cenário sem acrobacia |
| 480 s, 8 aeronaves, 5 giros cada | nenhuma caiu nem congelou; pior AGL **564 m** (a mais baixa da pilha), contra ~1.539 m sem acrobacia |
| determinismo 1/2/4 threads T/C | dumps **byte-idênticos** (4.000 frames), mais a repetição de 4 threads |
| não-regressão do gate no cenário de produção `flight` | todos os campos físicos do dump **exatamente iguais**; nenhum `bt=` divergiu |

**O custo em altitude é físico, e diminuir o stick piora.** Um tonneau só de aileron não tem
compensação de profundor, e invertido o *altitude hold* comanda o profundor no sentido contrário.
Um `slowRollStick` menor faz o giro durar mais — mais tempo perdendo altitude, não menos.

A altitude **comandada** durante a manobra continua passando por `clampAltitudeToTerrain()`, mas
a aeronave **afunda abaixo do comandado** durante o giro: o piso protege o alvo, não a trajetória.
Quem encurtar os intervalos, ou voar mais perto do terreno, precisa refazer a medição de AGL.

### Sobre a não-regressão do `a4ap.xml`

O gate foi comparado A/B contra o `a4ap.xml` original, na fixture `intruder` de `flight`
(3.000 frames, 2 threads): **posição, altitude, rumo, banco, arfagem, velocidade e combustível
saem exatamente iguais**, e nenhum rótulo `bt=` mudou. A única diferença em 120 linhas foram 14
valores de `trackRange` no **último dígito impresso** (~1e-9 m em ~20 km) — acrescentar um nó
`<switch>` ao grafo do FCS perturba o estado físico abaixo da precisão impressa, e o filtro
alfa-beta do track manager amplifica isso até o último decimal. Não é ruído de execução: duas
execuções da mesma configuração saem byte a byte idênticas.

## Corrigido: colisão com o terreno depois de múltiplos giros

O teste de 480 s acima ("nenhuma caiu nem congelou") era curto demais para pegar o defeito.
Rodando o MESMO cenário por 4000 s (200.000 frames, `-deterministic`), as 8 aeronaves colidem,
uma a uma, entre t=680 s e t=1142 s — `agl` cruza 0 e `dec` congela (`Player::setMode(CRASHED)`
do framework, que para a aeronave de decidir).

**Dois defeitos reais, investigados separadamente — e a medição corrigiu a hipótese inicial
sobre qual dos dois decidia o resultado.**

**1) `( Navigate )` nunca aplicava o piso anti-CFIT — bug independente do giro.**
`bt_nodes::DecisionContext::clampAltitudeToTerrain()` já era aplicado por Patrol/RTB/Support/
SlowRoll, mas `NavigateAction::tick()` comandava a altitude ESTÁTICA do steerpoint direto, sem
passar por ele. Como o perfil da rota é um número escolhido pelo autor do cenário sem visibilidade
do banco de elevação real, um trecho onde o relevo verdadeiro é mais alto do que o perfil assumia
levava a aeronave, em voo reto e nivelado — **sem giro nenhum acontecendo no instante do
impacto** —, direto para dentro do terreno. Confirmado com um diagnóstico temporário: depois de um
giro, a árvore volta para `NAV` a uma altitude compatível com o alvo pretendido pela rota, mas
`( Navigate )` não corrigia para cima mesmo com o piso (`elevação + terrainClearance`) já
ultrapassando essa altitude por centenas de metros. Corrigido: `NavigateAction::tick()` agora
passa a altitude comandada por `clampAltitudeToTerrain()`, igual aos outros nós.

**2) `domain::AerobaticPlan` não tinha nenhuma borda de altitude antes de começar um giro** — um
giro custa altitude (ver "Medido" acima), e nada impedia um novo sorteio de vencer com a aeronave
já baixa. Corrigido com um terceiro parâmetro em `update()`, `safeToRoll`: na borda Idle→Rolling,
sem a folga de `slowRollMinMargin` sobre o piso anti-CFIT
(`bt_nodes::DecisionContext::hasAerobaticAltitudeMargin()`), a manobra fica **adiada** — nunca
cancelada, nunca reagendada: o relógio trava em zero e a manobra dispara assim que a folga
aparecer. Uma manobra **já em curso nunca aborta no meio**, mesmo que a margem suma depois de
começar (a própria manobra derruba a altitude) — terminar a um banco arbitrário, possivelmente
invertido, seria mais perigoso do que fechar o giro; o `slowRollTimeout` de sempre continua sendo
a única saída de emergência de uma manobra que nunca fecha.

**Medido, não presumido: o fix (2) sozinho NÃO mudou o resultado.** Rodando com
`slowRollMinMargin` em 900 m e depois em 1500 m, os dois com o fix (1) já aplicado, os dois deram
**exatamente as mesmas colisões, nos mesmos frames, com a mesma contagem de giros por aeronave**
— prova de que nenhum giro estava de fato sendo adiado pela borda: todo sorteio já vencia com
folga de sobra. O mecanismo real de cada colisão (investigado com um diagnóstico temporário no
código, removido depois): **um único** giro derruba a aeronave, e a recuperação de altitude
seguinte não é monotônica — é um comportamento oscilante (o fenômeno de fugoide de qualquer asa
fixa, trocando velocidade por altitude e vice-versa), às vezes chegando perto de ~4-5 m/s de
subida líquida, bem abaixo do `maxClimbRateMps: 40` do `Autopilot`. Quando esse vale da oscilação
coincide com um trecho onde o relevo real sobe (o mesmo relevo que o fix 1 passou a respeitar
continuamente, não só no instante do giro), a aeronave chega a colidir mesmo tendo sido
COMANDADA a subir bem acima de onde estava.

**O que de fato eliminou as colisões, medido A/B**: elevar `terrainClearance` deste cenário do
default do modelo (500 m) para 1200 m explícito. Diferente de `slowRollMinMargin` (que só olha o
piso no INSTANTE de começar um giro novo), `terrainClearance` eleva o piso que `( Navigate )`
mantém **o tempo todo**, inclusive fora de qualquer giro — é isso que dá margem tanto contra o
custo do giro quanto contra a variação normal do relevo ao longo da rota. Com 900 m de
`slowRollMinMargin` mas ainda 500 m de `terrainClearance`, as 8 aeronaves colidiam (mesmos frames
de sempre); com `terrainClearance` em 1200 m, **zero colisões em 300.000 frames** (6000 s
simulados) — pior AGL entre as 8 aeronaves, 800 m (contra os 564 m do teste de 480 s "sem
acrobacia crítica" documentado acima).

`slowRollMinMargin` **continua valendo a pena manter** mesmo não tendo sido o fator decisivo
aqui: é uma rede independente contra um giro sucessivo sem tempo de recuperação (nunca exercitada
por esta semente/seed específica, mas real para outra combinação de intervalo/terreno), e o custo
de mantê-la é zero — o slot é opcional e o comportamento sem ele já era seguro por outro caminho.

**Medido depois dos dois fixes:** as mesmas 8 aeronaves, 300.000 frames (6000 s simulados) —
**zero colisões** (`agl` nunca cruza 0, `dec` nunca congela, pior AGL 800 m). Determinismo com 1,
2 e 4 threads T/C confirmado byte-idêntico (`tests/determinism/check_determinism.sh`, 2000
frames). `make test` do modelo segue verde, com seis testes novos exercitando as duas bordas
diretamente: `tests/domain/test_AerobaticPlan.cpp` (`SemSafeToRollAManobraFicaAdiada`,
`ComecaNoTickEmQueSafeToRollViraTrue`, `SafeToRollNaoAfetaManobraJaEmCurso`),
`tests/tree/test_flight_tree_random.cpp` (`SemMargemDeAltitudeAdiaAManobra`,
`ManobraEmCursoNaoAbortaSeAMargemSumirDepois`) e `tests/tree/test_flight_tree_nav.cpp`
(`AltitudeComandadaRespeitaOPisoDeTerrenoQuandoORelevoEMaisAltoQueARota`).

## Verificação

```bash
# determinismo: 1, 2 e 4 threads T/C tem de dar dumps byte-identicos
tests/determinism/check_determinism.sh ./build/app/src/app A4-6DOF-RANDOM 2000 "" \
  sandbox/A4-6DOF-RANDOM/configs/scenario_a4_6dof_random.edl.in

# determinismo do PROPRIO SORTEIO de acrobacia: a MESMA patrolMasterSeed tem
# que reproduzir com 1, 2 e 4 threads, e DUAS sementes diferentes tem que
# divergir -- em qualquer numero de threads (prova que quem muda o giro e a
# semente, nao o agendamento entre threads). Ver tests/determinism/
# check_random_seed_scenario.sh -- mesma propriedade que check_patrol_seed.sh
# ja prova para domain::PatrolPlan/flight, script a parte porque este
# cenario nao e' alcancavel por make_fixture.py (frota a4_1..a4_8, nao
# falcon1..4).
tests/determinism/check_random_seed_scenario.sh ./build/app/src/app A4-6DOF-RANDOM \
  sandbox/A4-6DOF-RANDOM/configs/scenario_a4_6dof_random.edl.in 8000
```

**Medido rodando** (8000 frames = 160 s, sementes 111111/222222): as duas propriedades batem —
`a-1==a-2==a-4` e `b-1==b-2==b-4` (mesma semente, qualquer numero de threads), e `a-N!=b-N` para
N=1/2/4 (sementes diferentes divergem, em qualquer numero de threads). A divergência é de
verdade, não numérica: na primeira linha onde os dumps diferem (`frame=2500`), `a4_5` já está em
`bt=ROLL` com a semente A e ainda em `bt=NAV` com a B — a semente muda **quando** cada aeronave
rola, não só a nona casa decimal da física. Contagem de linhas `bt=ROLL` no dump inteiro: 37
(semente A) contra 27 (semente B), com 1 thread.

O que olhar no Tacview: o giro tem de **passar de ~72° de banco** e fechar os 360°. Se
travar entre 50° e 72°, o gate do nivelador não está aplicado — é o modo de falha mudo que o
`slowRollTimeout` transforma em sintoma.
