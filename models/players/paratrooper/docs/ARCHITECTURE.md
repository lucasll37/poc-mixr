# Arquitetura do modelo `paratrooper`

## O que este modelo é

O corpo físico e a decisão de UM paraquedista: queda livre → paraquedas aberto → pousado. Nada
mais — não pilota nada, não tem sensor, não decide combate. A pergunta que este modelo responde é
"quando abrir o paraquedas e quando considerar que já pousou", e a física de cada estágio.

**O que este modelo explicitamente NÃO faz** (fora de escopo desta tarefa, adiado para depois):
nenhum cenário VERSIONADO o liga ao `models/players/C-130` — `src/poc/c130-airdrop` continua
largando o placeholder (`C130ParatrooperPlaceholder`), e o swap é tarefa futura, puramente de EDL.
Hoje o modelo só é demonstrado sozinho (`src/poc/paratrooper-drop`), com os paraquedistas já
declarados em queda a partir de uma altitude inicial. O lado do MODELO da liberação, porém, já
está pronto e medido contra o C-130 de verdade — ver "O ponto de saída" abaixo.

## Por que `Effect`, e não `LifeForm`

A classe concreta (`xnative::Paratrooper`) deriva de `mixr::models::Effect`
(`AbstractWeapon → Player`) — o mesmo idioma nativo de `Chaff`/`Decoy`/`Flare`, e da classe que
`models/players/C-130` já usa hoje como placeholder (`C130ParatrooperPlaceholder`, também
`Effect`). Duas razões, as duas medidas contra o fonte do MIXR antes de escrever qualquer código:

1. **`LifeForm` é auto-grudado ao terreno.** `Player::positionUpdate()` clampa incondicionalmente
   qualquer player cujo `getMajorType()` seja `GROUND_VEHICLE|SHIP|BUILDING|LIFE_FORM` à elevação
   do terreno — correto para um soldado JÁ pousado, mas errado durante queda livre/velame (ele
   "voaria" grudado ao chão, nunca caindo de verdade). `AbstractWeapon`/`Effect` têm major type
   `WEAPON`, fora dessa máscara — não são clampados em nenhum estágio, o que deixa a física INTEIRA
   (inclusive o pouso, que aqui é feito por congelamento manual de velocidade, não por clamp
   nativo) sob controle deste modelo.
2. **Compatibilidade de EDL com o mecanismo que já existe em C-130.** `xnative::
   ActionParatrooperRelease` (em `models/players/C-130`) já libera qualquer `AbstractWeapon*` cujo
   `Player::getType()` bata com uma string configurável (default `"PARATROOPER"`) — **sem
   `dynamic_cast`** para nenhuma classe concreta. `xnative::Paratrooper` já nasce com `type:
   "PARATROOPER"` por padrão (mesma convenção do placeholder), então o swap futuro (trocar
   `C130ParatrooperPlaceholder` por `Paratrooper` dentro do `stores:` de `c130-airdrop`) é **só
   EDL** — a classe concreta declarada na estação + `provides:` — sem tocar C++ nenhum em C-130.
   Ver `models/players/C-130/docs/ARCHITECTURE.md`, seção "A liberação de paraquedista".

## A FSM de estágio — mão única, não Schmitt trigger

`domain::Stage{FREEFALL, CANOPY, LANDED}` + `domain::next(stage, aglM, profile)`
(`include/domain/ParachuteFsm.hpp`). Diferente do Schmitt trigger que `models/template`
demonstra (que pode "desengajar" de volta), as transições aqui são **de mão única**: uma vez em
`CANOPY`, nunca volta a `FREEFALL` mesmo que a AGL suba (ex.: sobrevoar um vale sob o velame);
`LANDED` é absorvente. Isso simplifica a regra (nenhuma histerese é necessária — o próprio
one-way já evita "flutter") e é o que faz `domain::next()` ser uma função **total**, testável
exaustivamente sem estado global (`tests/domain/test_ParachuteFsm.cpp`).

**Onde o avanço acontece — e por que NÃO dentro de um nó da árvore.** `ParatrooperBtBehavior::
genAction()` chama `domain::next()` **uma vez** por ciclo de decisão, antes de tickar a árvore. Os
dois nós da árvore (`IsStage`/`SetStage`) só LEEM o estágio já avançado — nunca mutam. Isso é
deliberadamente diferente do template, cujo `ExampleThresholdCondition` avança E lê dentro do
mesmo nó — seguro lá só porque aquela árvore nunca avalia mais de UMA condição por tick. Aqui, no
pior caso (`FREEFALL`), o `Fallback` tenta `IsStage LANDED` (falha) e depois `IsStage CANOPY`
(falha) antes de cair no ramo incondicional — se cada condição avançasse a FSM, o estágio
avançaria duas vezes no mesmo frame.

## A árvore

`Fallback` de 3 ramos por prioridade — igual à forma do `flight_tree.xml` de produção do A-4 (a
primeira branch que der `SUCCESS` vence):

```
1) LANDED (Sequence: IsStage LANDED  -> SetStage LANDED)
2) CANOPY (Sequence: IsStage CANOPY  -> SetStage CANOPY)
3) FREEFALL (SetStage FREEFALL, incondicional -- ramo de degradacao)
```

Dois nós apenas (`bt::nodes::IsStageCondition`, `bt::nodes::SetStageLabelAction`), reusando o
mesmo idioma do template ("um nó parametrizado por port, usado em vários ramos"): `IsStage` é
comparação pura contra `stage()`; `SetStage` chama `decision().take(label)`. Os rótulos
(`"FREEFALL"`/`"CANOPY"`/`"LANDED"`) vêm de `domain::labelOf()`/`stageFromLabel()` — fonte ÚNICA
de verdade, evitando um enum duplicado e uma tabela de mapeamento reescrita em mais de um lugar
(o `bt::DecisionContext` não precisa de um método a mais para isso, ao contrário do que pareceria
necessário à primeira vista).

## A física por estágio, em `xnative::Paratrooper::weaponDynamics()`

`AbstractWeapon::dynamics()` (fase 0 do frame de tempo crítico) chama `weaponGuidance()` +
`weaponDynamics()` — ambos stubs vazios na classe base, feitos para serem sobrescritos por um
`Effect`/`Bomb`/etc — e SÓ DEPOIS `Player::dynamics()` (que faz `positionUpdate()`, a integração
de posição a partir da velocidade que `weaponDynamics()` acabou de fixar).

- **`FREEFALL`**: chama `BaseClass::weaponDynamics(dt)` — a física nativa do próprio `Effect`
  (arrasto linear + gravidade: `accel = -dragIndex·v; accel[DOWN] += g; v += accel·dt`), sem
  reimplementar nada. O único ajuste é o valor de `dragIndex`: o default de `Effect` (`0,0006`)
  converge para uma terminal de ~16 km/s (puro balístico); `0,18` converge para
  `v = g/dragIndex ≈ 54,5 m/s` — queda livre humana ventre-para-baixo plausível.
- **`CANOPY`**: reta pra baixo, taxa constante (`canopyDescentRate`, default 5,5 m/s) — SEM
  deriva de vento. Limitação documentada, não bug: adicionar deriva lateral realista é extensão
  futura (exigiria um modelo de vento e uma fonte de variação determinística, ex.: `libs/xrandom`
  — não feito aqui por não ter sido pedido).
- **`LANDED`**: velocidade `(0,0,0)`. Isso zera `vp` (a magnitude total, recalculada dentro de
  `setVelocity()`) — e o guard `vp > 0` de `Player::positionUpdate()` vira um no-op permanente,
  travando a posição **sem tocar `setMode()`** e sem passar pela cascata de detonação.
- Nos dois últimos estágios, `setEulerAngles(0,0,heading)` roda ANTES de `setVelocity()` — ordem
  que importa: `setEulerAngles()` muta a matriz de rotação que `setVelocity()` usa para recalcular
  a velocidade em eixos do corpo. Sem isso, o paraquedista herdaria a atitude nariz-baixo que
  `Effect::weaponDynamics()` produz quando a velocidade de solo cai a zero (`atan2` da componente
  vertical contra velocidade horizontal zero).

## O ponto de saída — 15 m atrás e 10 m abaixo da aeronave

Quem libera um paraquedista é o mecanismo nativo de arma (`Stores::releaseWeapon()` →
`AbstractWeapon::release()` → `this->clone()`), e é o **clone** — não o objeto declarado no
`stores:` — que nasce ao lado da aeronave. Entre a liberação e o primeiro frame como jogador de
verdade, esse clone fica em `PRE_RELEASE`, e é aí que `AbstractWeapon::dynamics()` decide onde ele
nasce:

```
pos0b = ( getInitPosition().x(), getInitPosition().y(), -getInitAltitude() )
pos   = posição_da_aeronave + pos0b * matriz_de_rotação_da_aeronave   // "body to earth"
```

**A armadilha está aí, e não é óbvia**: para uma arma em `PRE_RELEASE`, `initXPos`/`initYPos`/
`initAlt` **não são** posição no terreno de jogo, como são para um player declarado direto em
`players: {}` — são um deslocamento em eixos do **corpo da aeronave lançadora** (x para o nariz,
y para a asa direita, altitude com sinal +para cima). Um `.edl` que não os declare deixa os três
em zero, que é o default nativo — e o paraquedista nasce **colado** à aeronave, na posição exata
dela.

`xnative::Paratrooper` sobrescreve `dynamics()` para fixar esses dois valores enquanto
`PRE_RELEASE`, imediatamente antes de `BaseClass::dynamics()` lê-los: por padrão, **15 m atrás e
10 m abaixo** (`releaseOffsetAft`/`releaseOffsetBelow`, os dois `<Distance>`, ajustáveis por
cenário; valor negativo é aceito sem clamp e põe o paraquedista à frente/acima, o que não faz
sentido para um salto mas é decisão de cenário, não invariante deste modelo).

Três consequências que caem desse ponto de inserção, e são a razão de ser ele:

- **A rotação fica com o código nativo.** "Atrás" acompanha rumo, arfagem e rolamento da aeronave
  sozinho — nenhuma trigonometria própria neste modelo. Medido: a mesma aeronave com rumo 0° põe o
  paraquedista 15 m ao sul; com rumo 270°, 15 m a leste.
- **A velocidade continua sendo a da aeronave**, herdada do ramo nativo logo abaixo
  (`setVelocity(getLaunchVehicle()->getVelocity())`) — quem sai pela porta sai com a velocidade do
  avião, não parado no ar.
- **Um paraquedista declarado direto em `players: {}` não é afetado.** É o caso de
  `src/poc/paratrooper-drop`, que nunca passa por `PRE_RELEASE`: lá os `initXPos`/`initYPos`/
  `initAlt` continuam significando posição absoluta, intocados.

O par também é copiado em `copyData()` — sem isso o clone que de fato voa nasceria com o default,
e um cenário que ajustasse os slots seria ignorado em silêncio.

**Medido rodando**, num cenário de bancada (não versionado) com o C-130 de `models/players/C-130`
liberando um `Paratrooper` de verdade pelo `StoresMgr`, contra um controle com os dois offsets
zerados (o comportamento de antes desta mudança): **15,001 m atrás e 9,998 m abaixo** com o
default, e 40,003 m / 24,995 m com os slots em 40/25 — os dois em eixos do corpo da aeronave, que
naquele instante voava com 15° de rolamento e 6° de arfagem. A comparação é DIFERENCIAL contra o
controle de propósito: uma leitura absoluta carrega o atraso de um frame entre o posicionamento
(fase 0) e a amostragem do `MsgFeed` (laço de fundo), ~1,5 m a 77 m/s.

## A rede de segurança contra o `CRASH_EVENT` genérico

`Player::dynamics()` dispara `CRASH_EVENT` sempre que `getAltitudeAgl() < 0` para qualquer player
local de major type `AIR_VEHICLE|WEAPON|SPACE_VEHICLE` — o MESMO mecanismo que causa um avião
"crashar" no terreno. Para um `AbstractWeapon`, o handler padrão é
`AbstractWeapon::crashNotification()`, que RESPEITA `isCrashOverride()`; mas `Effect` sobrescreve
`crashNotification()` de novo e **ignora** essa flag, sempre chamando `killedNotification()`
(`KILL_EVENT` para os subcomponentes, dano/fumaça/chamas = 1,0) e `setMode(DETONATED)` —
semanticamente errado para um soldado pousando em segurança.

`xnative::Paratrooper` sobrescreve `crashNotification()`/`collisionNotification()` mais uma vez:
respeita `isCrashOverride()` (restaura o contrato que `Effect` quebra) e, se não estiver
suprimido, apenas chama `setJumpStage(LANDED)` — sem cascata nenhuma. Isso é uma REDE DE
SEGURANÇA, não o caminho principal: em condições normais, `domain::next()` já transiciona para
`LANDED` (via `groundAgl`) antes da AGL real cruzar zero. Ela existe para o caso de um frame de
atraso entre a física (fase 0) e a decisão (fase 3) — ver "Duas cópias do estágio" abaixo.

**`updateTOF()` também precisou de sobrescrita.** O mecanismo de TOF de `AbstractWeapon` é
puramente temporal, independente da AGL: `Effect` reduz o `maxTOF` herdado (60 s) para 10 s — bem
menos que um salto inteiro (queda livre + velame facilmente passa de 60-90 s). O construtor de
`Paratrooper` sobe isso para 300 s, e `updateTOF()` para de incrementar (nunca expira) uma vez
`LANDED` — sem isso, um paraquedista parado no chão numa simulação suficientemente longa se
autodetonaria ao vencer o teto, mesmo tendo pousado em segurança há muito tempo.

## Duas cópias do estágio, e por que elas convergem

O estágio existe em DOIS lugares: `ParatrooperBtBehavior::stage_` (a DECISÃO, avançada por
`domain::next()` a cada ciclo) e `xnative::Paratrooper::stage_` (a ATUAÇÃO, escrita só por
`ParatrooperAction::execute()` → `setJumpStage()`, e também pela rede de segurança de
`crashNotification()`). Em condições normais, a Action aplica no mesmo frame o que a Behavior
decidiu — as duas cópias ficam sincronizadas com um atraso de no máximo um frame (a Action do
frame N aplica a decisão calculada no início do frame N; a física do frame N+1 já reflete o novo
estágio).

Se a rede de segurança disparar (o `CRASH_EVENT` chegando antes do ciclo de decisão reagir), só a
cópia NATIVA (`Paratrooper::stage_`) muda imediatamente para `LANDED` — a cópia da Behavior
(`ParatrooperBtBehavior::stage_`) converge sozinha no PRÓXIMO ciclo, porque `domain::next()` é
monotônico e `AGL < 0 ⟹ AGL ≤ groundAgl`. As duas nunca ficam permanentemente divergentes.
Documentado aqui porque não é óbvio e é exatamente o tipo de coisa que alguém "corrige" achando
que é um bug.

**Restrição a respeitar ao configurar `groundAgl`**: precisa ser maior que
`canopyDescentRate × duração do frame` — com os defaults (5,5 m/s, frame de 0,02 s a 50 Hz), isso
é ~0,11 m; `groundAgl: 2 m` dá uma folga de ~18×. Configurar `groundAgl` perto de zero aumenta o
risco de a rede de segurança (o `CRASH_EVENT` genérico) disparar antes da FSM alcançar `LANDED`
por conta própria — não é incorreto (a rede de segurança cobre exatamente esse caso), mas é menos
"limpo" que a transição normal.

## Terreno — por que existe o slot `requireTerrain`

`Player::reset()` zera `tElev`/`tElevValid`; a primeira atualização de verdade só acontece no
primeiro `updateData()` de FUNDO (10 Hz) — antes disso, `getAltitudeAglM()` devolve a altitude MSL
crua (uma **superestimativa** da AGL real, já que `tElev == 0`). Isso é seguro na direção
"nunca abre o paraquedas cedo demais por engano" (a AGL aparente é sempre MAIOR que a real nesse
intervalo), mas ainda é uma leitura que não reflete o terreno de verdade.

`ParatrooperBtBehavior` ganhou o slot `requireTerrain` (default ligado) especificamente para
isso: enquanto `isTerrainElevationValid()` for falso, o avanço da FSM fica em espera (o estágio
não muda, mas a árvore continua tickando e a `Action` continua escrevendo no `xboard` — nada
trava). Desligar (`requireTerrain: 0`) é para bancada/teste sem `WorldModel`/terreno nenhum (ver
`tests/native/`, que roda inteiramente sem esse gate).

## O agente — decide na fase 3, nunca no laço de fundo

`xnative::ParatrooperAgentTC` é uma cópia estrutural de `models/players/C-130/src/xnative/
FlightAgentTC.cpp` (que por sua vez adapta `models/players/A-4`), resolvendo as mesmas três
armadilhas documentadas lá: `AgentTC` não é construído por fábrica nenhuma do MIXR (registro
manual); `AgentTC::updateTC()` chama `controller()` em TODA fase, sem filtro (aqui, filtrado para
só rodar na fase 3, com o `dt` do frame inteiro); `Agent::updateData()` também chama
`controller()` (aqui, sobrescrito como no-op) — sem essa terceira correção, a FSM avançaria até
cinco vezes por frame (quatro fases + o laço de fundo). Este repositório não reintroduz o padrão
de decisão em thread de fundo (`( SimAgent )`) — todo modelo novo decide na fase 3, mesmo um tão
simples quanto este.

## Contrato de fábrica

Cinco classes, sem prefixo (nenhuma colide hoje com `Flight*` de A-4, `C130*` de C-130, ou
`Example*` do template/mirror): `Paratrooper`, `ParatrooperAgentTC`, `ParatrooperState`,
`ParatrooperBtBehavior`, `ParatrooperAction`. Rodar `python3 tests/guard/check_colisao_fabrica.py`
(raiz) depois de qualquer mudança em `xnative/factory.cpp`.

## Ler também

- [`../../C-130/docs/ARCHITECTURE.md`](../../C-130/docs/ARCHITECTURE.md) — o mecanismo de
  liberação genérica que este modelo já nasce compatível com, e o contrato de swap futuro
- [`../../template/docs/ARCHITECTURE.md`](../../template/docs/ARCHITECTURE.md) — a separação em
  camadas (`domain/`→`bt/`→`ubf/`→`xnative/`) que este modelo segue
- [`../../../CLAUDE.md`](../../../CLAUDE.md), seção "O MODELO é um plugin, construído numa etapa
  PRÉVIA" — visão geral de `models/`
