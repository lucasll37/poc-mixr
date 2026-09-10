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

## Verificação

```bash
# determinismo: 1, 2 e 4 threads T/C tem de dar dumps byte-identicos
tests/determinism/check_determinism.sh ./build/app/src/app A4-6DOF-RANDOM 2000 "" \
  sandbox/A4-6DOF-RANDOM/configs/scenario_a4_6dof_random.edl.in
```

O que olhar no Tacview: o giro tem de **passar de ~72° de banco** e fechar os 360°. Se
travar entre 50° e 72°, o gate do nivelador não está aplicado — é o modo de falha mudo que o
`slowRollTimeout` transforma em sintoma.
