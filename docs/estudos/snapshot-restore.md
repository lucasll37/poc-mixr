# Salvar uma simulação no meio e retomá-la byte-idêntica

> **Escopo.** Este documento responde a uma pergunta específica: é possível salvar uma simulação
> deste repositório num instante arbitrário e retomá-la depois, de modo que duas execuções — uma
> contínua e outra retomada do salvamento, partindo da mesma semente — terminem **byte-idênticas**?
>
> É um estudo de viabilidade, não um plano de obra. Nenhuma linha de código foi escrita para ele.
> Toda afirmação sobre o framework foi verificada no fonte e traz `arquivo:linha`; onde não deu
> para confirmar, o texto diz que não deu.

## 1. A resposta

**Sim — por reexecução, não por captura de estado, e só entre execuções em `-deterministic`.**

A forma que funciona é o **replay determinístico**: o "save" não guarda o estado da simulação;
guarda um *manifesto* (cenário, hashes do binário e dos plugins, sementes, número de frames e um
journal das entradas externas). O "load" reconstrói o instante **reexecutando** os N frames num
processo novo. A byte-identidade sai *por construção* — é a mesma computação com as mesmas
entradas.

A resposta óbvia, serializar o estado e reinjetá-lo, **não funciona neste projeto**, e não por
volume de dados: por acessibilidade. Há pelo menos seis bloqueios, e três deles bastariam sozinhos.

E há uma limitação honesta, que atinge em cheio o caso de uso mais provável: **não dá para salvar
de dentro de uma sessão interativa e retomar byte-idêntico**, porque tempo real e passo fixo são
duas simulações diferentes (§5.1).

### 1.1 O que cada motivação ganha, e o que não ganha

| motivação | atendida? | por quê |
|---|---|---|
| **auditoria / reprodutibilidade** | **sim, integralmente** | é o caso de uso natural do replay: o artefato arquivado É a receita da execução, e reconstruí-lo *prova* a reprodutibilidade em vez de assumi-la |
| **ponto de partida para RL** | **sim** | partir episódios de um instante "aquecido" é reexecutar até ele; e o `src/rl` já roda em passo fixo, sob comando do Python, que é exatamente o regime onde a garantia vale |
| **depuração / what-if** | **em parte** | reproduzir o instante, sim; *variar um parâmetro e comparar* também. O que não dá é o scrub barato — cada salto paga a reexecução inteira (§7.3), e não há estado para inspecionar ou editar (§7.2) |
| **sessão longa interrompida** | **não** | é exatamente o caso que §5.1 exclui: uma sessão interativa roda o laço de fundo a 10 Hz contra a thread de tempo crítico a 50 Hz, e uma retomada em passo fixo roda os dois a 50 Hz. São trajetórias diferentes |

A assimetria dessa tabela é o resultado mais importante do estudo, e vale ser dito sem rodeio: a
motivação mais intuitiva — *fechar o app hoje e continuar amanhã de onde parei* — é justamente a
que a arquitetura viável não entrega, e a razão não é falta de esforço, é a diferença estrutural
entre os dois regimes de execução.

## 2. Por que serializar o estado não funciona

### 2.1 O integrador do JSBSim não tem caminho público de restauração

Este é o bloqueio mais fundamental, porque atinge a física.

`FGPropagate` — o propagador das equações de movimento — usa integradores **multi-step** por
padrão (`FGPropagate.cpp:93-96`):

```
integrator_translational_rate     = eAdamsBashforth2
integrator_translational_position = eAdamsBashforth3
integrator_rotational_rate        = eRectEuler
integrator_rotational_position    = eRectEuler
```

Um integrador multi-step não é função só do estado atual: ele lê o **histórico das derivadas
anteriores**. Esse histórico vive em quatro `std::deque` de tamanho fixo 5, dentro de
`struct VehicleState` (`FGPropagate.h:98-137`): `dqPQRidot`, `dqUVWidot`, `dqInertialVelocity`,
`dqQtrndot`. São cerca de 65 doubles por aeronave — pouca coisa, e perfeitamente serializável, se
houvesse por onde.

Não há.

- `GetVState()` (`FGPropagate.h:521`, público) **devolve** o struct inteiro, deques inclusive.
- `SetVState()` (`FGPropagate.h:523`, público; implementação em `FGPropagate.cpp:606-619`)
  **descarta** os quatro deques — e também `vQtrndot`, `vInertialVelocity` e `qAttitudeLocal`. Não
  é descuido não documentado: a linha 608, logo abaixo da assinatura, carrega um comentário do
  próprio autor — *"//ToDo: Shouldn't all of these be set from the vstate vector passed in?"*.
- `InitializeDerivatives()` (`FGPropagate.cpp:190-196`), chamado por `RunIC()`
  (`FGFDMExec.cpp:571`), faz `assign(5, derivada_atual)` — preenche o histórico com **cinco cópias
  iguais** da derivada instantânea.

A consequência da última é aritmética e vale escrever por extenso. O Adams-Bashforth 3 é

```
Integrand += (1/12)·dt·(23·d[0] − 16·d[1] + 5·d[2])
```

Com `d[0] = d[1] = d[2]`, o parêntese vira `(23 − 16 + 5)/12 = 12/12 = 1`, ou seja **Euler puro**.
O mesmo acontece com o AB2 (`1,5·d − 0,5·d = 1·d`). Toda restauração que passe pelo caminho de
reset **degrada o primeiro passo de integração a Euler**, e a diferença entra na trajetória.

A ordem de grandeza desse erro, estimada, é `0,5·ddot·dt²` — com `dt = 0,02 s` e uma derivada de
aceleração da ordem de 1 m/s³, algo como 2·10⁻⁴ m/s, que integrado dá ~10⁻⁵ m de posição.

Vale notar onde esse número já aparece: o `CLAUDE.md` registra que chamar `reset()` repetidamente
no `src/rl` deixa uma deriva de ~1e-5 m em `eastM` e ~1e-6 em `fuelFraction` por reset
(`src/rl/bindings/NativeSimulation.hpp:50-58`). A causa **dominante** dessa deriva, porém, é outra
e é pior — ver §2.6.

### 2.2 Boa parte do estado do FDM é privada e está fora da property tree

A property tree do JSBSim é enumerável e escrevível, o que sugere um caminho de captura. Ela não
cobre o que importa:

| estado | onde | acesso |
|---|---|---|
| `PreviousInput1/2`, `PreviousOutput1/2` de cada filtro digital | `FGFilter.h:225,232-233` | `private`, sem getter nem setter |
| `output_array` (a linha de atraso) | `FGFCSComponent.h:107-112` | `protected` |
| `Cranking` do motor | `FGEngine.h:212-215` | `protected`, sem setter (há `SetRunning`/`SetStarter`, não este) |

As duas aeronaves do repositório (`A4`, `c310`) declaram seis componentes de filtro/integrador cada
nos respectivos `*ap.xml`. E o `FGFDMExec` agrega **quinze** modelos (`FGFDMExec.h:230-246`):
propagação, entrada, inercial, atmosfera, sistemas/FCS, balanço de massa, auxiliar, propulsão,
aerodinâmica, reações de solo, reações externas, empuxo, aeronave, acelerações, saída. Cada um com
estado próprio.

### 2.3 `Simulation::execTime` não tem setter — nem protegido

`execTime` (`Simulation.hpp:263`) e `simTime` (`:269`) são privados. O bloco `protected` da classe
(`:235-247`) oferece `setCycle()`, `setFrame()`, `setPhase()`, `setEventID()` e
`setWeaponEventID()` — e mais nada. **Nem subclassando dá para restaurá-los**, e este projeto já
subclassa `Station` (`libs/xclock/ClockStation`), então a rota existiria se o framework a
permitisse.

`execTime` não é decorativo: é ele que data cada linha ACMI enviada ao Tacview
(`TacviewOutput.cpp:493`). Uma retomada com `execTime` diferente produz um replay de telemetria
deslocado no tempo.

### 2.4 Não há reconstrução de `Station` no mesmo processo

`libs/xplugin/PluginRegistry.cpp:117` mantém `sealed_`, um latch de mão única: depois do primeiro
`edl_parser()`, qualquer segunda tentativa de carregar módulo aborta com `std::exit()`. Não é uma
peculiaridade escondida — é a razão de existirem `app::respawnSelf()` (`app/src/app/Respawn.cpp`,
`execv()` de si mesmo) e a guarda `g_attemptedBuild` (`src/rl/bindings/StationBuilder.cpp:36`).

Ou seja: mesmo que todo o estado fosse capturável, **reinjetá-lo exigiria um processo novo** —
e nesse ponto a distância para o replay encolhe muito.

### 2.5 Há estado que não é C++

`libs/xpyembed/PyEmbed.cpp:155` mantém `Script::globaisPorPlayer`, um `std::map<int, PyObj>` — e
`PyObj` é `void*` (`PyEmbed.cpp:28`), ou seja um ponteiro cru para objeto do CPython. É o
dicionário de globais de cada aeronave dentro do interpretador Python embarcado. Na poc
`python-flight`, esse dicionário é onde vive a altitude de cruzeiro fixada na primeira decisão e o
estado da manobra de evasão em curso.

Serializar isso genericamente é serializar objetos Python arbitrários. Não há protocolo para isso
no repositório, e não há como haver sem restringir o que um script pode guardar.

### 2.6 E o `RESET_EVENT` também não devolve a simulação ao t=0

Este ponto é o contrapeso justo: o framework não oferece um "restaurar" nem para o caso mais
simples de todos, que é voltar ao início.

`JSBSimModel::reset()` (`JSBSimModel.cpp:772-870`) **não recria** o `FGFDMExec` — só o constrói se
for `nullptr` (`:791`) — e termina em `fdmex->RunIC()` (`:870`). E `RunIC()`
(`FGFDMExec.cpp:560-595`) **não** chama `ResetToInitialConditions()`, que é quem chamaria
`InitializeModels()` (`:296-305`), que é quem roda `InitModel()` em **todos** os quinze modelos.

Consequência direta e verificável: `FGPropulsion::InitModel()` nunca roda, logo **o combustível não
é reabastecido entre resets**. Cada `reset()` executa um frame de aquecimento
(`primeStation()` → `tcFrame(0,02)`) que queima combustível, e esse combustível não volta. É a
explicação acumulativa que falta para a deriva de ~1e-6 em `fuelFraction` por reset — e a de
`eastM` vem junto, porque massa diferente é empuxo/aceleração diferentes.

Junto disso, o estado interno do FCS sobrevive ao reset (§2.2), e `fdmex->GetSimTime()` nunca volta
a zero.

### 2.7 O `Player` guarda a mesma pose muitas vezes

`models::Player` mantém, simultaneamente, cerca de 31 campos que codificam a mesma pose 6-DOF
(`Player.hpp:950-995`): geodética escalar (`latitude`/`longitude`/`altitude`), NED (`posVecNED`),
ECEF (`posVecECEF`), velocidade em quatro formas (incluindo `velVecN1` — estado **n−1** explícito),
aceleração em três, Euler NED e Euler ECEF com seus caches de seno e cosseno, quaternion `q` e três
matrizes de rotação. Mais `syncState1`/`syncState2`, um duplo buffer cuja metade "atual" depende da
paridade do frame (`:1077-1080`).

Todos privados. E não adianta capturar um subconjunto mínimo e recomputar o resto: a recomputação
passa por `sin`, `cos`, `sqrt` e normalização de quaternion, e a ordem das operações num
recomputador externo não é a mesma do `positionUpdate()` do framework. Para byte-idêntico, ou se
capturam os 31, ou não se captura.

### 2.8 Resumo de §2

Serializar estado exigiria, no mínimo: um patch no JSBSim (que este projeto *builda do fonte*, via
`deps/jsbsim/conanfile.py`, então é tecnicamente possível) **e** um fork do MIXR (que este projeto
consome como pacote binário Conan, e cuja premissa explícita é não ser objeto de desenvolvimento)
**e** um protocolo de serialização para objetos Python arbitrários **e** um processo novo de
qualquer forma, por causa do `sealed_`.

E o serializador resultante envelheceria em silêncio a cada campo novo do framework ou do modelo.

## 3. A arquitetura que funciona: replay determinístico

### 3.1 A ideia

O arquivo de save contém:

```
manifesto  { caminho do .edl.in, sha256 do .edl.in, valores dos tokens de template,
             sha256 do executável, sha256 de cada .so de plugin, sha256 do tile .hgt,
             sementes, N de frames, -threads }
journal    { a sequência de entradas externas consumidas nos N frames }
```

O restore é: validar os hashes, montar as mesmas opções, e rodar N frames em `-deterministic` num
processo novo. Continuar dali é seguir rodando.

### 3.2 Por que isso entrega byte-identidade

Porque **o repositório já testa exatamente essa propriedade, e ela passa**.

`tests/determinism/check_determinism.sh` invoca o binário dentro da função `roda()` (`:104`), que é
chamada quatro vezes (`:138`, `:141`) — quatro **processos separados**. A linha `:155` compara os
pares, e o primeiro deles é `"threads-4 threads-4b"`: duas execuções na **mesma** configuração,
comparadas com `diff -q` sobre as linhas `^frame=` (`:157`).

Se reexecutar N frames num processo novo não reproduzisse o estado, esse teste estaria vermelho
hoje. Ele não está.

### 3.3 O que o replay elimina, em vez de resolver

Esta é a parte contraintuitiva e é o argumento central do documento. Todo estado global de processo
que assombraria um restore in-place simplesmente **não é um problema** aqui, porque num processo
novo ele nasce no valor inicial e reacumula pela mesma trajetória:

| estado | por que seria problema num restore | por que não é no replay |
|---|---|---|
| `Simulation::execTime` | sem setter, nem protegido (`Simulation.hpp:263`) | começa em 0 e só faz `execTime += dt` (`Simulation.cpp:462`) — N frames o reproduzem |
| `xboard::g_board` | mapa global sem função de reset (`libs/xboard/Board.cpp:17`), e alimenta `bt=`, `dec=`, `alert=`, `sent=`, `recv=` no dump | nasce vazio, reacumula igual |
| `MetaObject::count/mc/tc` | `int` cru sem atomicidade (`MetaObject.hpp:31-33`) | não aparece em campo nenhum do dump |
| `BT::getUID()` | `static uint16_t`, dá o `uid_` **const** de cada nó (`tree_node.cpp:19-23`) | reinicia; a atribuição continua sendo uma bijeção |
| `TrackManager::nextTrkId` | contador monotônico (`TrackManager.hpp:103`) | reinicia |
| `xplugin::sealed_` | latch de mão única (`PluginRegistry.cpp:117`) | processo novo, parse novo |
| os quatro deques do Adams-Bashforth | `SetVState()` os descarta (§2.1) | **nunca precisam ser restaurados — são reconstruídos rodando** |

A última linha é a que mais importa: **o replay não encosta no JSBSim**.

### 3.4 O único PRNG do repositório já é reproduzível

Há exatamente um gerador de números aleatórios em todo o projeto: o `std::mt19937_64` de
`domain::PatrolPlan` (`models/players/A-4/include/domain/PatrolPlan.hpp:79`). O MIXR não tem PRNG
nativo.

E a semente dele já é derivada do **nome** do player, nunca da ordem de descoberta
(`models/players/A-4/src/ubf/BtBehavior.cpp:96-118`, via `libs/xrandom/DeterministicRng.hpp`):

```
instanceSeed = deriveSeed(patrolMasterSeed, fnv1a64(player->getName()))
seed final   = deriveSeed(instanceSeed, kPatrolJitterSalt)
```

Isso foi desenhado para sobreviver ao paralelismo do pool de tempo crítico, e é justamente o que
faz o PRNG sobreviver também a uma reexecução — sem nenhum trabalho extra. `check_patrol_seed.sh`
já prova as duas metades: mesma semente reproduz entre 1/2/4 threads; sementes diferentes divergem.

### 3.5 O journal de rede, quando for a hora

O escopo imediato é cenário hermético (sem `networks:`), como todas as fixtures de teste já fazem.
Mas vale registrar que a porta para DIS **existe e é limpa**, porque isso decide se a arquitetura
tem teto:

`dis::NetIO::recvData()` (`interop/dis/NetIO.cpp:653-660`) é o **funil único** de entrada de PDU —
seus dois únicos chamadores são os laços de drenagem de `netInputHander()` (`:241`, `:428`). E os
slots aceitam **qualquer** `base::NetHandler`, sem checar tipo concreto:

```
ON_SLOT(1, setSlotNetInput,  base::NetHandler)     // interop/dis/NetIO.cpp:119-120
ON_SLOT(2, setSlotNetOutput, base::NetHandler)
```

`base::NetHandler` é abstrata com seis virtuais puros (`base/network/NetHandler.hpp:45-61`), e
`initNetwork(bool)` é virtual **não** pura — dá para escrever um handler que nunca abre socket
nenhum. É exatamente o padrão que `libs/xjoystick::JoystickIoHandler` já usa para o slot
`ioHandler:` de `linkage::IoHandler`.

O desenho do journal seria auto-sincronizante: o laço de `netInputHander()` é função determinística
dos tamanhos devolvidos por `recvData()`, então reproduzir a **sequência de retornos** (incluindo
os zeros) reproduz a forma do laço, sem precisar de número de frame.

Bônus: o framework já oferece um índice absoluto, reproduzível entre execuções, para ancorar o
journal — `getExecCounter()`, o contador de fases de tempo real desde a partida, **público**
(`Simulation.hpp:186`). `cycle()`, `frame()` e `phase()` (`:180-182`) também são públicos, mas são
**cíclicos** (`frame()` conta 0..15, `phase()` 0..3) — quem serve de índice absoluto é o
`getExecCounter()`.

## 4. O que precisaria ser construído

Esboço, para dimensionar — não é especificação. **Nenhuma linha do MIXR é tocada, e nenhum nome de
fábrica novo do modelo é criado**, então `provides:` não muda em cenário nenhum e
`models/template/src/mirror.cpp` não precisa de ajuste. Todo o código novo é do host.

**Fase 1 — hermético, que é o recorte pedido.**

| peça | onde |
|---|---|
| gravação e validação do manifesto | `app/include/app/RunManifest.hpp` + `app/src/app/RunManifest.cpp`, um arquivo por questão, no molde de `app/ScenarioTemplate.*` |
| opções `-save-manifest`, `-resume` | `app/include/app/Options.hpp` + `app/src/app/Options.cpp`; `-resume` implica `-deterministic` e **recusa** se qualquer hash divergir |
| ramo de retomada | `app/src/main.cpp` |
| teste | `tests/determinism/check_resume.sh` + `test()` em `tests/meson.build`, suíte `determinism`, `is_parallel: false` |

Detalhe que não é óbvio: **o manifesto tem de hashear o `.edl.in`, nunca o `.generated.edl`**.
`app::generateScenario()` expande `@RUN_ID@`, que é timestamp + PID (`app/src/main.cpp:109-116`),
então o arquivo gerado é diferente a cada execução por construção. E `PluginDescV1::build_id`
**não** serve de guarda de integridade: o próprio header o marca como "SÓ DIAGNÓSTICO"
(`libs/xplugin/PluginAbi.hpp:88-90`) — tem de ser hash de arquivo do `.so`.

**Fase 2 — DIS, se e quando.**

| peça | onde |
|---|---|
| `JournalNetHandler` (subclasse de `base::NetHandler`, slots `file:`, `mode:`, `inner:`) | `libs/xdisjournal/`, molde exato de `libs/xjoystick/meson.build` |
| encadear a factory | uma linha em `app/src/mixr_factory.cpp` |
| fixture que preserva `networks:` trocando o `netInput:` | modo novo em `tests/scenario/make_fixture.py` |

**Fase 3 — fechamento das frestas**, tudo em configuração, quase nada em C++:
`simulationTime:`/`day:`/`month:`/`year:` nos cenários (§5.2); `timeline: EXEC` nos dois lados de um
par DIS (§5.3); `setenv("PYTHONHASHSEED", "0", 1)` antes de `Py_InitializeEx`
(`libs/xpyembed/PyEmbed.cpp:138`), que é uma linha; e as guardas de §7.

## 5. Condições de contorno

A garantia não é incondicional. Estas são as condições, apuradas uma a uma.

### 5.1 Só vale entre execuções no MESMO regime — e é a limitação que dói

**Tempo real e passo fixo são simulações diferentes, não a mesma simulação em velocidades
diferentes.**

Em `-deterministic`, `tcFrame(dt)` e `updateData(dt)` andam **1:1**, com o mesmo `dt = 1/tcRate`
(`app/src/app/DeterministicRun.cpp:21,29,39`). Ou seja: o laço de fundo roda a **50 Hz**.

Em tempo real, a thread de tempo crítico nativa roda a 50 Hz por conta própria, enquanto o laço de
fundo roda a **10 Hz** (`src/node/Run.cpp:82` — `const double dt{0.1}; // 10 Hz, a mesma taxa de
fundo usada em toda a base`; o mesmo vale para `app/src/app/DashboardLoop.cpp`).

E não é uma diferença cosmética, porque coisas que afetam o dump vivem no laço de fundo:

- **elevação de terreno** — `Player::updateElevation()` é chamada de dentro de
  `Player::updateData()` (`Player.cpp:630`), e alimenta os campos `elev=` e `agl=` do dump;
- **entrada DIS** — `Station::processNetworkInputTasks()` roda em `updateData()`;
- **`MsgFeed`** e as **`Action` de `Route`**.

Com cadências diferentes, as trajetórias divergem. O `README.md` da poc já declarava isso antes
deste estudo: *"Não prova: que o modo de tempo real é reprodutível bit a bit contra outra
execução"* (`src/poc/dis/flight/README.md:1612`).

**Consequência prática:** salvar no meio de um voo pilotado, ou de uma sessão longa no `./app`, e
retomar em passo fixo **não reproduz** — e nenhuma quantidade de journaling de entrada conserta
isso, porque o problema não são as entradas, é o entrelaçamento entre as duas threads.

Não é impossível *em princípio*: bastaria o journal gravar também o entrelaçamento T/C×fundo e o
replay obedecê-lo, ou o modo interativo passar a rodar `tcFrame`/`updateData` em lockstep — o que
mataria o propósito dele. É trabalho de outra ordem e fica fora do recorte recomendado.

### 5.2 O cenário precisa fixar o tempo de simulação, se tiver `Route`/`Action`

`Simulation::reset()` semeia `simTime` e `pcTime` do **relógio de parede**
(`Simulation.cpp:317-338`) e `simTimeSlaved` fica `true`, porque nenhum dos EDL do repositório
declara
`simulationTime:`/`day:`/`month:`/`year:`. Duas execuções do mesmo cenário já diferem em `simTime`
hoje — só não aparece porque o dump não o expõe.

Há **um** caminho pelo qual isso alcança o dump: `ActionDecoyRelease` compara
`interval < (tod - startTOD)` com `tod` vindo de `getSimTimeOfDay()` (`Actions.cpp:611,642-643`). O
offset absoluto cancela na subtração, mas a **magnitude** de `simTime` muda o espaçamento de ULP
dessa diferença, e há o wrap de meia-noite. Como a liberação de um store muda a massa da aeronave,
isso chega na física.

Só existe hoje em `src/poc/built-in_mixr_1`. Fecha-se declarando os quatro slots no cenário e
registrando os valores no manifesto.

### 5.3 DIS contra um simulador de terceiro fica com uma fresta

Registrado aqui porque decide o teto da arquitetura, mesmo estando fora do escopo imediato.

Além de `simTime`, existe `pcTime` — e este **não tem escape hatch nenhum** (os slots
`simulationTime:` etc. só tocam `simTime`). Ele entra no dead reckoning da entrada DIS:
`entityStatePdu2Nib()` faz `setTimeUtc(getSysTimeOfDay())` (`Nib_entity_state.cpp:51`) e, se o LSB
do timestamp do PDU estiver ligado — indicando timestamp *absoluto* —, `currentTime` passa a ser
`getTimeUtc()` (`:106-110`), gerando o `diffTime` que alimenta `resetDeadReckoning()` e o
`smoothVel`/`smoothTime` que posicionam o fantasma.

E absoluto é o **default** de qualquer emissor MIXR, porque `timeline` nasce `UTC`
(`interop/common/NetIO.hpp:404`; `interop/dis/NetIO.cpp:680-681`).

Com o peer sob nosso controle — o caso de `src/poc/dis/bandit` — fecha-se declarando
`timeline: EXEC` nos dois lados. Contra um simulador de terceiro não há saída limpa: só reescrevendo
o LSB do timestamp dentro do replay, e aí o journal deixa de ser fiel ao fio.

### 5.4 O save vale enquanto o binário valer

Recompilar o host, recompilar um `.so` de modelo, trocar o tile SRTM ou editar o `.edl.in` invalida
o save. Isso não é defeito: é a natureza de um mecanismo que reconstrói por reexecução.

O que importa é que **`-resume` recuse em vez de rodar e mentir**. Daí os hashes no manifesto. O
teste `plugin-hotswap` já existe justamente para provar que trocar o `.so` muda `hdg=` com o mesmo
`.edl` — ou seja, o modo de falha é real e já é conhecido aqui.

**Não consegui confirmar** reprodutibilidade entre máquinas diferentes. O `meson.build` raiz não
passa `-ffast-math` nem `-march=native`, e não encontrei despacho SIMD em runtime — mas isso é
ausência de evidência, não prova. Um save deve ser tratado como válido na máquina que o gerou até
que alguém meça o contrário.

## 6. Como provar

No molde de `check_determinism.sh` (fixture hermética derivada, `rc` checado **à parte** do `grep`,
diagnóstico da primeira divergência) e de `check_patrol_seed.sh` (o par positivo/negativo, que é o
que impede o teste de passar vacuamente).

`tests/determinism/check_resume.sh <binário> <rótulo> <N-save> <M-extra> [poc]`:

1. **Run contínuo** — `-deterministic $((N+M))`, filtra `^frame=` para `continuo.txt`.
2. **Run save** — `-deterministic N -save-manifest man.json`.
3. **Run resume**, em processo novo — `-resume man.json -deterministic $((N+M))` para
   `retomado.txt`.

**Asserção A — a propriedade pedida.** `diff -q continuo.txt retomado.txt`, comparando o arquivo
**inteiro**, não só as linhas com `frame > N`. Se olhasse só a cauda, um erro na reexecução dos N
primeiros frames que "convergisse de volta" passaria — e a arquitetura estaria mentindo exatamente
sobre o que promete.

**Asserção B — que é reexecução, não coincidência.** Repetir o passo 3 com 1, 2 e 4 threads. Os três
têm de ser idênticos entre si e idênticos ao contínuo.

**Asserção C — controle negativo, sem o qual o verde não vale nada.** Retomar com um
`patrolMasterSeed` diferente (`make_fixture.py --patrol-seed` já existe) e **exigir divergência**.
É o mesmo desenho de `check_patrol_seed.sh`.

**Asserção D — guarda de integridade.** Retomar com um `.so` substituído (basta copiar
`libtemplate_mirror.so` por cima, como `plugin-modelo-estranho` já faz) e exigir que o binário
**recuse com `rc != 0`**. Um save que roda contra um binário diferente é pior que um save que falha.

**Asserção E — cobre a ressalva de §5.1.** Salvar em tempo real e retomar em `-deterministic` tem de
ser **recusado** pelo binário. Não há como fazer isso reproduzir; deixar disponível seria vender uma
propriedade que a arquitetura não tem.

### Custo de retomada

Com o `~200×` que o `CLAUDE.md` registra para o modo `-deterministic` — **número herdado, não medido
neste estudo** — a aritmética dá:

| simulação | retomada (estimativa) |
|---|---|
| 1 h | ~18 s |
| 5 h | ~1,5 min |
| 24 h | ~7 min |

Linear em N, mas com duas ressalvas medidas no fonte: `RealtimeTelemetryServer::beginFrame()` faz um
`ofstream::flush()` a cada bloco de timestamp (`libs/xtacview/RealtimeTelemetryServer.cpp:191-198`)
— com `dataLogTime: ( Seconds 0.1 )` são 10 flushes por segundo simulado, e a 200× isso vira ~2.000
flushes por segundo de parede; e uma retomada de 24 h escreve na ordem de centenas de MB de `.acmi`.
O custo de I/O provavelmente domina o de CPU.

## 7. O que fica de fora, honestamente

**7.1 Não retoma de sessão interativa.** §5.1. É provavelmente o caso de uso que motiva a pergunta,
e a arquitetura não o entrega.

**7.2 Não há estado para inspecionar nem editar.** O save é um manifesto e, na fase 2, um journal de
bytes de rede. Não há posição, combustível, atitude, histórico de integrador,
`PatrolPlan::legTimer_` nem `ThreatPolicy::cmd_` em lugar nenhum do arquivo. Não dá para abrir o
save e ver onde a aeronave
estava, nem para "salvar, mexer numa variável, retomar". É o inverso exato da força de um snapshot —
e vale dizer que a única arquitetura que entregaria isso é justamente a que §2 mostra ser inviável.

**7.3 O custo é O(N), toda vez.** Voltar 10 segundos numa missão de 24 h custa os mesmos ~7 minutos
de uma retomada do fim. Um uso de *scrub* ("me leve para t=3h… agora t=3h05") é inviável: cada
pedido paga a reexecução inteira. Dez saves espalhados numa missão longa são dez reexecuções
independentes, não dez leituras baratas.

**7.4 Joystick não é retomável.** O `ioHandler:` é sondado no laço de fundo a 10 Hz, e
`-deterministic` nem chama `inputDevices()`. Uma sessão pilotada à mão cai inteira em 7.1.

**7.5 Reprodutibilidade não é correção.** Vale a mesma ressalva que
`src/poc/dis/flight/README.md:1616-1618` já faz para o teste de determinismo: um modelo que erra
sempre igual retoma perfeitamente e continua errado.

### 7.6 Três minas latentes

Nenhuma dispara hoje — cada uma foi conferida. Mas todas quebrariam a garantia se alguém as ligasse,
e nada no repositório vigia isso ainda. São candidatas naturais a guardas em `tests/guard/`.

| mina | evidência | estado hoje |
|---|---|---|
| **`rand()` global do JSBSim** em ruído de sensor e turbulência, mais o `static double V1, V2, S` de `GaussianRandomNumber()`, compartilhado entre todas as aeronaves e não thread-safe | `FGSensor.cpp:184-187`; `FGWinds.cpp:216`; `FGJSBBase.cpp:217-240` | **inerte** — o único `<sensor>` com `<noise>` do repositório está inteiramente comentado (`c310ap.xml:45-58`) e não há turbulência ativa |
| **`relWpnId++`** em `unsigned short` sem atomicidade, com a lista de players ordenada por ID (`insertPlayerSort`) — duas armas liberadas no mesmo frame por players em threads diferentes recebem IDs em ordem dependente de escalonamento | `Simulation.cpp:787-790,1074` | **não dispara** — `flight`/`python-flight`/`onnx-policy` não têm armas, e em `built-in_mixr_1` só `falcon1` libera. (Nota: `relWpnId`/`eventID`/`eventWpnID` são `unsigned short` e dão wrap em 65536 numa corrida longa — determinístico, mas semanticamente quebrado) |
| **`netRate: > 0`** num cenário tiraria a rede de dentro de `updateData()` para uma thread própria, e a sequência de `recvData()` deixaria de estar amarrada ao frame | `Station.hpp:266` (default é 0) | **não acontece** — e vale corrigir de passagem: o comentário da slottable em `Station.cpp:43` diz "default: 20 hz" e está **desatualizado** |

E uma quarta, mais barata de fechar do que de lembrar: `libs/xpyembed` chama `Py_InitializeEx(0)`
sem fixar `PYTHONHASHSEED` (`PyEmbed.cpp:138-143`), então a randomização de hash de string do
CPython é por processo. Hoje é inócuo — os quatro `.py` de `src/poc/python-flight/configs/policy/`
não iteram `dict` nem `set` — mas o replay depende de reexecução em processo **novo**, então pinar
é uma linha e tira a fragilidade de vez.

### 7.7 O que se descobriu por acidente

Dois achados que não são sobre save/restore, mas apareceram na investigação e valem para o projeto:

1. **A deriva de `reset()` do `src/rl` tem causa identificada** (§2.6): `RunIC()` não chama
   `ResetToInitialConditions()`, logo `FGPropulsion::InitModel()` nunca roda e **o combustível não é
   reabastecido entre resets**. O `CLAUDE.md` registra o sintoma (~1e-6 em `fuelFraction`, ~1e-5 m
   em `eastM` por reset) sem a causa; aqui ela está, e é acumulativa por construção.
2. **`Simulation::getExecCounter()` é público** (`Simulation.hpp:186`) e dá um índice **absoluto**
   de fases de tempo real desde a partida — ao contrário de `cycle()`/`frame()`/`phase()`
   (`:180-182`), que são cíclicos. Útil para qualquer coisa que precise correlacionar eventos entre
   execuções, não só para este estudo.

## 8. As outras arquiteturas avaliadas

Três alternativas foram estudadas a fundo antes de a recomendação se fixar no replay. Nenhuma
sobrevive, mas cada uma ensina algo — e duas quase funcionam, o que vale registrar para ninguém
refazer o caminho.

### 8.1 Checkpoint opaco de processo (CRIU / DMTCP)

**Congelar o processo inteiro em disco.** É, entre as candidatas, a única que entrega
byte-identidade *sem escrever uma linha de serialização*: um restore que recoloca todas as páginas
anônimas nos mesmos endereços devolve de graça exatamente o que trava as outras — os deques do
Adams-Bashforth, os `PreviousInput` privados do FCS, `execTime`, o pool de emissões da antena, o
`mt19937_64` do `PatrolPlan`, o `g_board`, os globais Python. Dispensa até o `sealed_`, porque o
processo restaurado já está *depois* do parse.

Reprova pelo ambiente e pela natureza da coisa. **Medido nesta máquina:**

```
CONFIG_CHECKPOINT_RESTORE=y      CONFIG_MEM_SOFT_DIRTY=y      CONFIG_NAMESPACES=y
# CONFIG_INET_DIAG_DESTROY is not set          ← o que sustenta o --tcp-close do CRIU
criu: Installed: (none)   Candidate: (none)    ← não existe no apt do Ubuntu 24.04
```

Kernel `6.18.33.2-microsoft-standard-WSL2`. Ter as opções do kernel é condição *necessária*, jamais
suficiente — e a que falta é justamente a dos sockets TCP, que é o que o cliente Tacview usa.

Mesmo que funcionasse: o artefato é opaco (não se versiona, não se inspeciona, não se move entre
máquinas) e **não sobrevive a um rebuild**, porque mapeamentos file-backed são restaurados por
caminho e offset, sem hash de conteúdo. E, sobretudo: é infraestrutura de sistema operacional, não
uma funcionalidade da aplicação.

*Duas premissas se dissolveram ao medir, e vale corrigir aqui:* não existe `mmap` em lugar nenhum
do MIXR — `SrtmHgtFile` aloca com `new` e lê com `ifstream`, que é a memória mais fácil de
checkpointar; e a ordem de retomada das threads T/C não ameaçaria o dump, porque
`Simulation::updateTC()` particiona os players por *stride de índice* e ressincroniza em
`waitForAllCompleted()` — o resultado já é invariante a escalonamento, coisa que
`check_determinism.sh` prova todo dia.

### 8.2 Snapshot in-process por `fork()` copy-on-write

**Chamar `fork()` no instante do save; o filho fica congelado, e "voltar" é retomá-lo.** A
byte-identidade sairia de graça — é o mesmo espaço de endereçamento — e ela contornaria todos os
bloqueios de §2 de uma vez. É a ideia mais elegante das cinco, e por isso merece ser reprovada com
cuidado.

**Não atende ao pedido, e isso é categórico:** `fork()` produz um *processo*, não um arquivo. Um
filho parado morre com o reboot, com um `kill`, com o fim da sessão. Não há nada em disco para
retomar amanhã. Custo colateral que passa despercebido: um filho congelado não é grátis em memória
— conforme o pai continua rodando, as páginas COW divergem e são duplicadas de verdade, então cada
save acumula RSS.

**E, mesmo como mecanismo de rewind dentro de uma sessão, o código de hoje não permite:**

| obstáculo | evidência |
|---|---|
| **O handshake do `ClockStation` é de mão única.** `tcStopRequested_` só é escrito com `true` (`ClockStation.cpp:102`); não existe `clearTcStop()`. Usá-lo para congelar antes do fork **mata a simulação do pai permanentemente** — ele foi escrito para o encerramento, não para pausa repetível | `libs/xclock/ClockStation.cpp:102,155`; `ClockStation.hpp:184` |
| **O pool de workers trava o filho no primeiro frame.** Os workers ficam parados em `pthread_mutex_t` comuns usados como semáforo binário, travados por *outra* thread. `fork()` duplica só a thread chamadora, então no filho `numTcThreads` continua > 0 mas os workers não existem — e o primeiro `tcFrame()` chama `waitForAllCompleted()` → `pthread_mutex_lock` num mutex travado por thread inexistente. Mutex default da glibc: sem `EDEADLK`, sem robustez. **Trava para sempre.** É o mesmo auto-deadlock que `ClockStation.hpp` já documenta para o shutdown | `SyncThread_linux.cpp:20-38,62-76,88`; `Simulation.cpp:249,258-259,562,569-570` |
| **Mutexes reais herdados travados.** `xlog::g_mutex` é tomado em *toda linha de log*, no destrutor de `Stream` — se a thread de fundo estiver dentro de um `LOG(...)` no instante do fork, o primeiro `LOG()` do filho trava. Idem `xboard::g_mutex`/`g_tagMutex`. E `Timer::semaphore` é `static`, process-wide. **Não há um único `pthread_atfork()` no repositório** — confirmado por varredura | `libs/xlog/Log.cpp:20,128-129`; `libs/xboard/Board.cpp:16,19`; `Timers.hpp:103` |
| **Sete descritores compartilhados.** `fork()` copia a tabela de fds, mas pai e filho apontam para a *mesma* descrição aberta — mesmo offset. O pior é silencioso: cada datagrama DIS é entregue a **exatamente um** dos dois processos, aleatoriamente; nenhum vê o tráfego completo, e as duas simulações divergem sem travar. Os dois `ofstream` (`.acmi`, `.jsonl`) ainda duplicam o buffer não descarregado, escrevendo linhas repetidas | `RealtimeTelemetryServer.cpp:132,75,103,111,147-150`; `MsgFileSink.cpp:67`; `PosixHandler.cpp:65,296,334` |

Há uma janela estreita em que o fork seria seguro — `-deterministic` **e** `-threads 1` **e** num
cenário sem `numTcThreads` literal — porque aí não há pool e a thread do laço é a própria chamadora.
Mas nem essa janela é limpa: `src/poc/dis/bandit/configs/scenario.edl:144` declara
`numTcThreads: 2` **literal**, sem token, então `-threads 1` não o alcança.

Duas suspeitas sobre bibliotecas de terceiros se dissolveram ao medir, e as duas **favorecem** o
fork — vale registrar, para não ficarem como fantasmas:

- **O ONNX Runtime não cria thread nenhuma com a configuração deste projeto.**
  `SetIntraOpNumThreads(1)` (`libs/xinfer/Infer.cpp:65-66`) vira `thread_pool_size = 1`, e o
  construtor de pool faz literalmente `if (options.thread_pool_size <= 1) { return nullptr; }`
  (`core/util/thread_utils.cc`, no fonte do ORT dentro do cache Conan) — não é "pool de tamanho 1",
  é pool **nenhum**; os operadores rodam inline na thread chamadora. Não há pool a orfanar. A
  ressalva honesta é que o ORT também **não tem um único `pthread_atfork`**: ele é fork-*tolerante*
  por acidente desta configuração, não fork-*safe* por projeto — trocar o `1` por qualquer valor
  maior reintroduziria o problema em silêncio.
- **O símbolo que faltava para o CPython existe.** `libs/xpyembed` não resolve
  `PyOS_AfterFork_Child` por `dlsym` (`PyEmbed.cpp:114-135`) — que é a função que o contrato do
  CPython exige do filho após um fork — mas a `libpython3.12.so.1.0` instalada **exporta** os três
  (`PyOS_AfterFork`, `_Child` e `_Parent`), ao lado de `Py_InitializeEx` e `PyEval_SaveThread`, que
  o repositório já resolve. Acrescentá-lo ao resolvedor é mudança de poucas linhas, e moveria o
  Python de "provavelmente funciona, sem contrato" para suportado.

**O que o fork ensina, e vale guardar:** ele é o único mecanismo do repositório que contorna o
latch `sealed_` do `PluginRegistry` sem `exec()` — o filho herda a `Station` já construída, sem
passar pelo parser. Se algum dia houver necessidade de duas `Station` vivas no mesmo host, é por
aí, não por reconstrução.

### 8.3 Híbrido: replay como fonte de verdade, retrato como oráculo

**O restore é replay; o arquivo carrega também um retrato do estado observável no instante do
save** — não para restaurar, e sim para *verificar* que o replay chegou no lugar certo.

A byte-identidade continua vindo 100% do replay; o retrato não contribui com nada para a garantia.
O que ele compra é **detecção de replay errado**, que é uma classe de falha diferente e que
acontece de verdade aqui: `.so` recompilado (o teste `plugin-hotswap` existe justamente para provar
que trocar o `.so` muda `hdg=` com o mesmo `.edl`), `.edl.in` editado, regime trocado, máquina
diferente. Sem oráculo, cada um desses produz uma trajetória plausível e errada, **em silêncio**;
com ele, produz um abort nomeando `(frame, player)`.

Custo estimado: cerca de uma pessoa-semana sobre o replay puro. Vale a pena? Provavelmente sim,
mais tarde — mas com uma ressalva conceitual que precisa ficar escrita: **o retrato é condição
necessária, nunca suficiente.** O dump é uma projeção lossy — `fixed(9)` sobre um `mach` da ordem
de 0,22 mostra 9 dígitos significativos contra os ~16 de um `double`, então uma divergência de
1 ULP passa invisível no frame N e pode dominar no frame N+5000. E o dump não enxerga nada dos
deques do `FGPropagate`, do blackboard da árvore, dos globais Python, do pool de emissões nem de
`nextTrkId`. "Verificado" e "idêntico" não são a mesma palavra.

Uma nota de precisão que este estudo herda dessa análise: o retrato **não pode ser editado**. Ele é
*saída* do replay, não entrada. Quem editar apenas faz o oráculo falhar — que é o comportamento
correto, mas não é o que a palavra "edição" promete.

### 8.4 Quadro comparativo

| arquitetura | veredito | byte-idêntico | risco / motivo |
|---|---|---|---|
| **A — replay determinístico** | **recomendada**, com ressalvas | **sim, por construção** | não cobre sessão interativa (§5.1); custo O(N) em toda retomada; sem estado para inspecionar |
| B — serializar estado | **inviável** | não | exigiria patch no JSBSim **e** fork do MIXR **e** protocolo para objetos Python **e** processo novo de qualquer forma; envelhece a cada campo novo (§2) |
| C — checkpoint de processo | **inviável** | sim, por construção | `CONFIG_INET_DIAG_DESTROY` ausente e `criu` sem candidato no apt; artefato opaco que não sobrevive a rebuild; é infra de SO, não funcionalidade (§8.1) |
| D — `fork()` COW | **inviável** para o pedido | sim, por construção | não persiste em disco; pool de workers trava o filho; mutexes herdados travados; 7 fds compartilhados (§8.2) |
| E — híbrido | viável | sim — mas vem 100% de A | complexidade sobre A; o retrato verifica, não restaura (§8.3) |

## Apêndice — método e limites deste estudo

**Como foi feito.** Levantamento do estado mutável do host, das libs `libs/x*` e do plugin `A-4`;
consulta ao fonte vendorizado do MIXR e ao fonte do JSBSim no cache Conan; e avaliação das cinco
arquiteturas acima. As afirmações decisivas foram reconferidas na implementação, não só no header —
o que já pagou: um dos levantamentos concluiu, lendo `FGPropagate.h`, que `Get/SetVState` dava um
caminho completo de captura e restauração da propagação; ler `FGPropagate.cpp:606-619` mostrou que
`SetVState()` descarta o histórico, o que **inverte** a conclusão (§2.1).

**Onde o fonte mora.** O do JSBSim v1.1.11 está em
`~/.conan2/p/jsbsicf75ee37bc65c/s/src/` — **não** em `contexts/src/mixr/deps/jsbsim/`, que só tem o
`conanfile.py`. O do MIXR está em `contexts/src/mixr/` e foi conferido por `diff` contra a cópia
que o CI compila: idêntico nos arquivos que sustentam o argumento.

**O que não foi feito, e é importante saber.** Nenhum binário foi executado — o estudo é de leitura
de fonte. Em particular:

- **o `~200×` do modo `-deterministic` é número herdado** do `CLAUDE.md`, e os tempos de retomada
  de §6 são aritmética sobre ele, não medição;
- **reprodutibilidade entre máquinas diferentes não foi confirmada** (§5.4). Há indícios a favor —
  o `meson.build` raiz não passa `-ffast-math` nem `-march=native`, e não se encontrou despacho
  SIMD em runtime — mas ausência de evidência não é prova;
- **o comportamento do CRIU neste kernel não foi testado**, só as opções de configuração;
- **o comportamento efetivo de um processo forkado** não foi testado — a análise de §8.2 é de
  leitura de fonte, e a contagem real de threads em runtime (que dependeria de inspecionar
  `/proc/<pid>/task`) não foi medida;
- **as verificações adversariais planejadas para as cinco arquiteturas rodaram parcialmente.** As
  conclusões de A, C, D e E foram checadas contra o fonte por mais de um caminho; as de B repousam
  no levantamento de estado, cujos pontos decisivos (§2.1, §2.3, §2.4) foram reconferidos um a um.

**Reprodutibilidade não é correção.** Vale fechar com a mesma ressalva que
`src/poc/dis/flight/README.md:1616-1618` já faz para o teste de determinismo, porque ela vale igual
aqui: um modelo que erra sempre igual retoma perfeitamente — e continua errado.
