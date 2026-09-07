# onnx-policy

> **ATUALIZAÇÃO — esta poc não tem mais executável próprio.** A camada de aplicação
> (`include/app/` + `src/app/` + `mixr_factory`, ~1.500 linhas que eram copiadas byte a byte em
> cada poc) saiu daqui: quem executa agora é o **`./app`**, o runner único —
> `./build/app/src/app -folder ./sandbox -scenario onnx-policy`. O que sobra nesta pasta é o **cenário**
> (`configs/`), os dados de execução (`data/`) e este README. Trechos abaixo que citam
> `src/app/…`, `main.cpp` ou `build/src/poc/…` descrevem a estrutura ANTERIOR — a explicação de
> cada etapa continua valendo, só que os arquivos moram em `app/src/app/`. Ver
> [src/poc/meson.build](../meson.build) para o porquê e para a prova de neutralidade (os dumps
> saíram byte-idênticos).

A [multi-thread](../multi-thread/) **inteira**, com **uma** diferença: quem decide não é uma árvore
de regras — é uma **rede neural**. Um MLP de 6.211 parâmetros, carregado de
[`configs/policy_barrier.onnx`](configs/) e inferido **dentro da fase 3 do frame de tempo
crítico**, sem Python no processo e sem um frame de latência.

```bash
make build
./build/app/src/app -folder ./sandbox -scenario onnx-policy           # Tacview Real-Time Telemetry na porta 1238; Ctrl+C encerra
./tests/determinism/check_determinism.sh ./build/app/src/app onnx-policy 2000 onnx-policy         # verifica o determinismo (1, 2 e 4 threads T/C)
```

> **Rode sempre a partir da raiz do repositório**: cenário, árvore, `.onnx`, dados do JSBSim, tile
> SRTM e gravação `.acmi` são resolvidos por caminho relativo.

**O ciclo de trabalho que esta poc existe para ter:**

```bash
src/poc/rl-training/.venv/bin/python3 src/poc/onnx-policy/tools/train_policy.py   # treina e exporta o .onnx
./build/app/src/app -folder ./sandbox -scenario onnx-policy                                                              # veja o voo mudado
```

Nada é recompilado entre as duas execuções — nem o host, nem o plugin. O `.onnx` é lido do disco na
primeira decisão.

---

## Índice

1. [O que é novo, e o que não é](#1-o-que-é-novo-e-o-que-não-é)
2. [A árvore tem uma folha só — e por quê](#2-a-árvore-tem-uma-folha-só--e-por-quê)
3. [O que a rede aprendeu](#3-o-que-a-rede-aprendeu)
4. [O contrato: 28 entram, 3 saem](#4-o-contrato-28-entram-3-saem)
5. [Por que a política não orbita: o corte dos 360°](#5-por-que-a-política-não-orbita-o-corte-dos-360)
6. [Determinismo](#6-determinismo)
7. [A rede TEM a última palavra — não há mais árbitro neste cenário](#7-a-rede-tem-a-última-palavra--não-há-mais-árbitro-neste-cenário)
8. [Degradação: sem `.onnx`, a aeronave continua voando](#8-degradação-sem-onnx-a-aeronave-continua-voando)
9. [Trocar a rede por uma política treinada de verdade](#9-trocar-a-rede-por-uma-política-treinada-de-verdade)
10. [Armadilhas confirmadas rodando](#10-armadilhas-confirmadas-rodando)
11. [O que foi medido rodando](#11-o-que-foi-medido-rodando)
12. [Como verificar tudo](#12-como-verificar-tudo)

---

## 1. O que é novo, e o que não é

**Não é novo** — já existia e não foi tocado:

| peça | onde |
|---|---|
| o motor de inferência (`open`/`shape`/`run`, sessão cacheada por caminho) | [`libs/xinfer`](../../../libs/xinfer/) |
| o nó de árvore `( OnnxPolicy )` | `models/players/A-4/src/bt/nodes/OnnxPolicyAction.cpp` |
| a desnormalização da ação (`unscaleCommand`) | [`libs/xrlbridge`](../../../libs/xrlbridge/) |
| a ordem canônica dos 28 campos | [`libs/xrlbridge/ObservationFields.hpp`](../../../libs/xrlbridge/ObservationFields.hpp) |
| a pilha inteira: `Aircraft` + `JSBSimModel` + `Autopilot` + radar + `AlertDatalink` + terreno | igual à das gêmeas |
| o plugin | o **mesmo** `libflight_tc.so` das gêmeas, byte a byte |

**É novo** — o que esta pasta acrescenta:

* um **subprojeto completo** em cima daquelas peças: cenário próprio, árvore própria, rede própria,
  portas próprias (Tacview **1238**, DIS **3005**), alvos de `make` e a bateria de testes de
  sempre. Antes havia um `flight_tree_onnx.xml` de **exemplo** no `models/players/A-4`, apontando para
  um `.onnx` de **pesos aleatórios**, exercitado só por um teste: dava para provar que a cadeia
  funciona, não para *voar* com ela e olhar o resultado no Tacview.
* uma rede que **de fato pilota**: treinada (por clonagem de comportamento) em vez de sorteada, com
  o erro contra a regra medido e escrito no `doc_string` do próprio `.onnx`.
* o **treinador versionado** ([`tools/train_policy.py`](tools/train_policy.py)), para o artefato
  binário no repositório ser reprodutível e para o que ele aprendeu estar escrito em código, e não
  só nos pesos.

**Nenhuma linha de C++ foi escrita para isto.** O host é uma cópia do da `multi-thread` com caminhos
e banner trocados — `tests/guard/check_duplication.sh` exige que continue byte a byte igual — e o
modelo não mudou. Trocar as regras pela rede foi trocar um caminho de arquivo no `.edl`.

---

## 2. A árvore tem uma folha só — e por quê

[`configs/flight_tree_onnx.xml`](configs/flight_tree_onnx.xml):

```xml
<Fallback name="root">
  <OnnxPolicy model="./src/poc/onnx-policy/configs/policy_barrier.onnx"
              normalized="true" label="ONNX"/>
  <Patrol/>
</Fallback>
```

A gêmea [python-flight](../python-flight/) troca as **folhas de ação** da árvore de produção e
mantém a forma dela (quatro ramos, condições em C++). Aqui a árvore some: uma política treinada
**é** o mapa observação → ação inteiro, incluindo a decisão de *quando* evadir. Manter os ramos
seria pedir à rede que decidisse dentro de um recorte que ela não conhece.

O segundo ramo não é decorativo: `( Patrol )` só roda se a rede falhar (arquivo ausente, forma
diferente de 28→3, erro de inferência) — ver a [seção 8](#8-degradação-sem-onnx-a-aeronave-continua-voando).

O preço dessa escolha é que o rótulo `bt=` deixa de ser um modo de voo e passa a ser sempre `ONNX`.
É por isso que esta poc **não entra** na lista `pocs` de `tests/meson.build` (os modos `intruder` e
`lowfuel` afirmam sobre `EVADE`/`SUPPORT`/`RTB`) e ganha testes próprios — ver a
[seção 12](#12-como-verificar-tudo).

---

## 3. O que a rede aprendeu

Clonagem de comportamento: amostra-se o espaço de observação, calcula-se o comando que uma **regra**
daria, e treina-se o MLP a reproduzi-lo. Não é RL — é o caminho mais curto entre "uma regra que eu
sei escrever" e "uma rede que voa", e serve ao propósito da poc, que é provar a **cadeia** (rede →
comando → `Autopilot`, dentro do frame), não descobrir uma tática nova.

A regra é uma **barreira no paralelo `norte = 0`**, com quebra ao contato:

| saída | regra |
|---|---|
| rumo | `90 + 90·tanh(norte / 4000)` — guiagem por **erro lateral**: ao norte da linha o comando tende a 180 (sul), ao sul tende a 0 (norte), sobre a linha é 90 (leste). Com contato, o mesmo termo apontado para o lado oposto ao do intruso: `90 + 90·tanh((norte_do_contato − norte) / 4000)` |
| altitude | `terreno + 900 m`, limitada a `[1500, 3200] m` — segue o relevo da Serra do Mar |
| velocidade | 160 kt de cruzeiro, 185 kt com contato (os mesmos `patrolSpeed`/`evadeSpeed` do cenário) |

Arquitetura: `28 → 64 → 64 → 3`, `tanh` em todas as camadas (o `tanh` final é o que mantém a ação
em `[-1,1]`, a mesma forma que um export do Stable-Baselines3 produz). 6.211 parâmetros, 25 KB de
`.onnx`, opset 17.

Erro contra a regra, no conjunto separado (impresso pelo treinador e gravado no `doc_string` do
`.onnx`):

```
headingDeg: medio 0.27, maximo 2.50    altitudeM: medio 6.61, maximo 42.87    speedKts: medio 0.08, maximo 1.01
```

E o que isso vira em voo — `falcon1` começa **9,3 km ao norte** da linha:

```
frame=  1000  n=8699.7  hdg=142.5      convergindo
frame=  8000  n=  91.0  hdg= 93.1      entrou na faixa de 100 m (t = 160 s)
frame= 20000  n=  -3.3  hdg= 90.0      seguindo a barreira para leste
```

`|norte| ≤ 11,1 m` entre os frames 15.000 e 20.000 (100 s de simulação): converge e **não oscila** —
o `tanh` é a própria saturação suave que evita o *overshoot*.

O treinador não tem uma lista de campos escrita nele: a ordem dos 28 vem do C++
(`src/poc/rl-training/tools/export_onnx.py`, que a lê de `mixr_gym._native`, que expande a X-macro de
`ObservationFields.hpp`). Uma cópia divergiria em silêncio e a rede voaria errado **sem erro
nenhum**.

---

## 4. O contrato: 28 entram, 3 saem

```
entrada  float32[1, 28]   o WorldView do frame, na ordem canônica, em unidade CRUA
saída    float32[1,  3]   [rumo, altitude, velocidade], normalizados em [-1,1]
```

O nó confere a forma antes de comandar; um `.onnx` com outra forma é recusado com
`LOG(ERROR)` e a `Patrol` assume. `normalized="true"` manda desnormalizar com
`xrlbridge::unscaleCommand()` (0–360°, 0–8000 m, 0–400 kt) — os **mesmos** limites que o treino usa
para montar o alvo. É o mesmo contrato do wrapper Gymnasium de [`src/rl`](../../rl/), então uma
política treinada lá entra aqui trocando o arquivo.

Para ver a ordem dos campos sem contar na mão:

```bash
python3 src/poc/rl-training/tools/export_onnx.py --campos
```

---

## 5. Por que a política não orbita: o corte dos 360°

A patrulha da gêmea `python-flight` é uma **órbita geométrica** (marcação para a base menos 90°).
Uma rede contínua **não consegue** representá-la, e isso não é falta de capacidade — é a forma do
contrato:

* o comando de rumo sai de um `tanh` mapeado linearmente em `[0, 360]`;
* uma órbita percorre **todos** os rumos, então existe um ponto do espaço de observação onde o alvo
  salta de 360 para 0;
* uma função contínua não tem esse salto — ela **interpola**. No ponto do corte a rede devolveria um
  rumo intermediário, isto é, uma curva para o lado errado, exatamente quando a aeronave cruza o
  corte.

Um script Python não tem esse problema porque não interpola nada. A barreira foi escolhida por
**não** ter corte: o alvo vive em `[0, 180]` e é função suave da posição.

A saída conhecida seria a rede emitir **seno e cosseno** do rumo em vez do rumo — mas isso muda o
contrato de 3 saídas, compartilhado com o treino de `src/rl` e com o `unscale` de `xrlbridge`. Fica
registrado como o caminho, caso um dia se queira uma política que orbite.

---

## 6. Determinismo

Quatro aeronaves inferindo **em paralelo**, na fase 3, compartilhando **uma** sessão do ONNX
Runtime (o cache de `libs/xinfer` é por caminho, e as quatro apontam para o mesmo arquivo):

```bash
./tests/determinism/check_determinism.sh ./build/app/src/app onnx-policy 2000 onnx-policy
```

```
OK   threads-4 == threads-4b
OK   threads-1 == threads-2
OK   threads-1 == threads-4
OK   uma decisao por frame, por aviao, nas 3 configuracoes
```

Dumps **byte-idênticos**. Duas escolhas de `libs/xinfer` sustentam isso, e as duas já estavam lá:
a sessão é criada com `intra_op`/`inter_op` em 1 thread e execução sequencial, e `run()` solta o
mutex antes de inferir (`Ort::Session::Run` é seguro para chamada concorrente). Fixar as threads é,
de quebra, **mais rápido** neste tamanho de modelo: 50,1 µs contra 76,0 µs.

---

## 7. A rede TEM a última palavra — não há mais árbitro neste cenário

**Isto mudou, e é uma regressão de segurança real, não cosmética.** Versões anteriores desta poc
enrolavam o `( BtBehavior vote: 50 )` num `( UbfArbiter )` junto com um
`( AltitudeSafetyBehavior vote: 90 )` nativo — um piso anti-CFIT independente, calibrado contra o
terreno, que sobrepunha qualquer comando de altitude perigoso vindo da árvore, voto contra voto,
sem que a rede precisasse saber disso. `src/poc/onnx-policy/configs/scenario.edl.in` **não faz mais
isso**: `behavior:` do `( FlightAgentTC )` aponta direto para o `( BtBehavior )`, sem `( UbfArbiter
)`/`( AltitudeSafetyBehavior )` no meio (ver o bloco de comentário "SEM ARBITRO" no topo do
arquivo).

`AltitudeSafetyBehavior`/`UbfArbiter` continuam existindo como classes — o `libflight_tc.so` ainda
as exporta e `provides:` deste cenário ainda as lista — só que **este** cenário não instancia mais
nenhuma das duas. `src/rl`/`src/poc/rl-training` continuam usando o árbitro de propósito (não
mudaram); a mudança é só aqui.

**A consequência real, sem suavizar:** enquanto o `.onnx` carrega e infere com sucesso, a rede tem
a última palavra sobre altitude. Não há mais um piso anti-CFIT independente por cima dela — uma
política que comande altitude contra a montanha **não é mais interceptada por ninguém**. O
`terrainClearance: ( Meters 800 )` que o `( BtBehavior )` ainda declara (ver a
[seção 4](#4-o-contrato-28-entram-3-saem)/o `.edl.in`) só entra em jogo no ramo de **FALLBACK** da
árvore — `< Patrol >`, que só assume se o `.onnx` **falhar ao carregar** (arquivo ausente, forma
diferente de 28→3, erro de inferência). Ele não protege uma inferência bem-sucedida porém ruim: se
a rede infere normalmente e devolve um comando de altitude ruim, esse comando vai para o
`Autopilot` sem revisão nenhuma.

O teste que antes travava essa proteção, `scenario-terrain-onnx-policy` (fixture `terrain`, com
`minAltitude` acima da altitude de cruzeiro, afirmando `bt=SAFETY` em 32/32 linhas — a prova de que
o voto 90 vencia a rede), **foi removido** de `tests/meson.build` junto com o árbitro: `bt=SAFETY`
é hoje **inalcançável** neste cenário, porque não há mais `( AltitudeSafetyBehavior )` para produzir
esse rótulo. Não sobrou um teste equivalente — a propriedade que ele provava não é mais verdadeira.

Trocar de política aqui **volta a exigir revalidar segurança de voo por conta própria** — a rede
tem de ser confiável sozinha, do jeito que uma árvore de regras nua também precisaria ser sem um
árbitro por cima. Isto **não é** mais "barato experimentar" no sentido de segurança garantida; é
barato no sentido operacional de "troca-se um caminho de arquivo, nada recompila" — os dois
sentidos eram verdadeiros antes, só o segundo continua.

---

## 8. Degradação: sem `.onnx`, a aeronave continua voando

Arquivo ausente, forma errada, erro de inferência → o nó devolve `FAILURE`, o `Fallback` cai na
`Patrol` nativa e a aeronave voa a patrulha em C++ configurada pelos slots `patrol*`/`legTime` do
cenário. Foi observado de verdade durante a construção desta poc (ver a
[armadilha 1](#10-armadilhas-confirmadas-rodando)):

```
[ERROR] [xinfer] falha ao abrir './src/poc/onnx-policy/configs/policy_barrier.onnx':
        Unsupported model IR version: 13, max supported IR version: 9
...
frame=100 player=falcon1 ... bt=PATROL
```

O sintoma no dump é `bt=PATROL` onde se esperava `bt=ONNX`; a causa está no `LOG(ERROR)` (console,
`data/logs/onnx-policy.log`, e a aba **Log** do `./app`).

---

## 9. Trocar a rede por uma política treinada de verdade

O treinador desta poc é uma conveniência, não o caminho principal. O caminho principal é
[`src/rl`](../../rl/) (o ambiente) treinado por [`src/poc/rl-training`](../rl-training/) (o
consumidor — venv próprio, separado do de `src/rl`, porque carrega dependências de treino que o
ambiente em si não precisa conhecer):

```bash
# acrescente as deps do seu algoritmo em src/poc/rl-training/requirements.txt
# (ex.: descomente stable-baselines3) antes de criar o venv
make venv-rl-training
# treinar (ver models/players/A-4/docs/POLITICAS.md, seção 2)
PYTHONPATH=./dist/python src/poc/rl-training/.venv/bin/python3 src/poc/rl-training/tools/export_onnx.py \
    --sb3 runs/ppo_falcon.zip -o src/poc/onnx-policy/configs/policy_barrier.onnx
./build/app/src/app -folder ./sandbox -scenario onnx-policy
```

Ou aponte o atributo `model` da árvore para outro arquivo e mantenha os dois. Em nenhum dos casos
algo é recompilado.

Para reproduzir a rede versionada (semente fixa, mesmos pesos) — o treinador desta poc só precisa
de `numpy`+`onnx`, então o `venv-rl` da biblioteca (que já traz `numpy`) mais um `pip install onnx`
avulso bastam, sem precisar do venv de treino inteiro:

```bash
make venv-rl
src/rl/.venv/bin/pip install onnx        # o venv de 'make venv-rl' traz só gymnasium+numpy
src/rl/.venv/bin/python3 src/poc/onnx-policy/tools/train_policy.py
```

---

## 10. Armadilhas confirmadas rodando

1. **`ir_version` do `.onnx`: o ONNX Runtime deste pacote Conan aceita até 9; o pacote Python
   `onnx` 1.22 grava 13 por padrão.** O sintoma não é um erro de exportação — é a poc voando com
   `bt=PATROL`, porque a recusa acontece em tempo de execução, dentro de `xinfer::open()`. O
   treinador fixa `modelo.ir_version = 8` (o mesmo do `policy_example.onnx` já versionado em
   `models/players/A-4`). **Vale para qualquer `.onnx` gerado hoje** — inclusive o
   `src/poc/rl-training/tools/export_onnx.py --random`, que não fixa a versão e produz um arquivo que este ORT
   recusa se o `onnx` instalado for recente.
2. **A política é dado do CENÁRIO, não do modelo.** Por isso `.onnx` e árvore moram em `configs/`
   desta poc, lidos por caminho relativo, e não em `dist/share/mixr-plugins/flight/` (que é onde
   `models/players/A-4` instala os **dele**). As duas coisas convivem: o teste `scenario-policy-onnx`
   continua rodando a árvore do modelo com pesos aleatórios sobre a `multi-thread`.
3. **Portas próprias, senão as pocs brigam.** Tacview **1238** e DIS `localPort` **3005** (1234
   single/multi, 1235 bandit, 1236 app, 1237 python-flight; DIS 3001/3002/3003/3004). Todo
   mundo escuta DIS em 3000 e ignora a própria porta de origem.
4. **Comentário de XML não aceita `--`.** A mesma classe de armadilha do parser EDL com acento: o
   traço duplo dentro de um comentário faz o parser recusar o arquivo. `xmllint --noout` no `.xml`
   antes de rodar economiza o ciclo.

---

## 11. O que foi medido rodando

| o quê | resultado |
|---|---|
| a rede decide em todos os frames | `bt=ONNX` em 100% das linhas do dump (nunca cai no `Fallback`) |
| convergência para a barreira | `falcon1`, de 9,3 km de erro lateral, entra na faixa de 100 m em **160 s** de simulação |
| regime permanente | `\|norte\| ≤ 11,1 m` por 100 s, sem oscilação |
| altitude | segue `terreno + 900 m` (limitada pelo `maxClimbRateMps: 8.0` do c310, como toda subida nesta poc-mixr) |
| erro da rede contra a regra clonada | rumo 0,27° médio (2,50° máx.), altitude 6,6 m (42,9 máx.), velocidade 0,08 kt (1,01 máx.) |
| determinismo | dumps byte-idênticos com 1, 2 e 4 threads T/C, mais repetição com 4 |
| piso anti-CFIT | **não existe mais neste cenário** — sem `( UbfArbiter )`/`( AltitudeSafetyBehavior )`, `bt=SAFETY` é inalcançável; ver a [seção 7](#7-a-rede-tem-a-última-palavra--não-há-mais-árbitro-neste-cenário) |
| degradação | `.onnx` recusado → `bt=PATROL` + `LOG(ERROR)`, sem abortar |
| custo da inferência | 50,1 µs médios (medido em `libs/xinfer`, para este mesmo MLP) — 0,25% de um frame de 20 ms |

---

## 12. Como verificar tudo

```bash
make test                    # inclui os três testes desta poc
./tests/determinism/check_determinism.sh ./build/app/src/app onnx-policy 2000 onnx-policy       # determinismo com 1, 2 e 4 threads, em cenário hermético
```

Os testes próprios (`tests/meson.build`):

| teste | o que trava |
|---|---|
| `scenario-onnx-policy` | a rede decidiu em todos os frames (`bt=ONNX`), os dumps de 1, 2 e 4 threads são byte-idênticos, e `dec` avança na taxa de `frame` |
| `memory-onnx-policy` | os contadores de instância do MIXR não crescem entre 500 e 1000 frames |

**Removido, e por que ele não foi só apagado da tabela em silêncio**: havia um terceiro teste,
`scenario-terrain-onnx-policy` (modo `terrain`: com o piso do `( AltitudeSafetyBehavior vote: 90 )`
acima da altitude de cruzeiro, **toda** linha saía `bt=SAFETY` — a prova de que o voto 90 vencia a
rede, voto 50). Ele foi removido de `tests/meson.build` **junto com o árbitro**: o cenário de
produção desta poc não instancia mais `( UbfArbiter )`/`( AltitudeSafetyBehavior )`, então
`bt=SAFETY` é hoje **inalcançável** — não há mais nada equivalente para esse teste travar. Ver a
[seção 7](#7-a-rede-tem-a-última-palavra--não-há-mais-árbitro-neste-cenário) para a consequência de
segurança que isso representa.

Mais os que valem para todas as gêmeas: `tests/guard/check_duplication.sh` (a camada de aplicação
continua byte a byte igual à da `single-thread`) e `tests/guard/check_falcons_estrutura.sh`
(`falcon1..4` com o mesmo esqueleto de slots).

---

## Onde está o quê

| | |
|---|---|
| a árvore | [`configs/flight_tree_onnx.xml`](configs/flight_tree_onnx.xml) |
| a rede | `configs/policy_barrier.onnx` (25 KB, versionado) |
| o treinador | [`tools/train_policy.py`](tools/train_policy.py) |
| o cenário | [`configs/scenario.edl.in`](configs/scenario.edl.in) |
| o motor de inferência | [`libs/xinfer/README.md`](../../../libs/xinfer/README.md) |
| o nó `( OnnxPolicy )` | `models/players/A-4/src/bt/nodes/OnnxPolicyAction.cpp` |
| o guia de políticas | [`models/players/A-4/docs/POLITICAS.md`](../../../models/players/A-4/docs/POLITICAS.md) |
| o wrapper de treino | [`src/rl/README.md`](../../rl/README.md) |
