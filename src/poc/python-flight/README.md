# python-flight

> **ATUALIZAÇÃO — esta poc não tem mais executável próprio.** A camada de aplicação
> (`include/app/` + `src/app/` + `mixr_factory`, ~1.500 linhas que eram copiadas byte a byte em
> cada poc) saiu daqui: quem executa agora é o **`./app`**, o runner único —
> `app -scenario python-flight`, ou `make run-python-flight`. O que sobra nesta pasta é o **cenário**
> (`configs/`), os dados de execução (`data/`) e este README. Trechos abaixo que citam
> `src/app/…`, `main.cpp` ou `build/src/poc/…` descrevem a estrutura ANTERIOR — a explicação de
> cada etapa continua valendo, só que os arquivos moram em `app/src/app/`. Ver
> [src/poc/meson.build](../meson.build) para o porquê e para a prova de neutralidade (os dumps
> saíram byte-idênticos).

A [multi-thread](../multi-thread/) **inteira**, com **uma** diferença: as **leis de voo** não estão
compiladas em lugar nenhum. São quatro arquivos `.py` em [`configs/policy/`](configs/policy/),
lidos em tempo de execução e avaliados **dentro da fase 3 do frame de tempo crítico** — mesma
thread, mesmo tick, sem processo nem soquete no meio.

```bash
make build
make run-python-flight         # Tacview Real-Time Telemetry na porta 1237; Ctrl+C encerra
make check-python-flight       # verifica o determinismo (1, 2 e 4 threads T/C)
```

> **Rode sempre a partir da raiz do repositório**: cenário, árvore, scripts, dados do JSBSim, tile
> SRTM e gravação `.acmi` são resolvidos por caminho relativo.

**O ciclo de trabalho que esta poc existe para ter:**

```bash
make run-python-flight                       # veja o voo no Tacview
$EDITOR src/poc/python-flight/configs/policy/patrol.py
make run-python-flight                       # veja o voo mudado
```

Não há compilação entre as duas execuções. Nem do host, nem do plugin — o `.py` é lido do disco
na primeira decisão de cada aeronave.

---

## Índice

1. [O que é novo, e o que não é](#1-o-que-é-novo-e-o-que-não-é)
2. [A árvore: condição em C++, ação em Python](#2-a-árvore-condição-em-c-ação-em-python)
3. [Os quatro scripts](#3-os-quatro-scripts)
4. [O contrato: 28 floats entram, 3 saem](#4-o-contrato-28-floats-entram-3-saem)
5. [Estado por aeronave, e por que ele é seguro](#5-estado-por-aeronave-e-por-que-ele-é-seguro)
6. [O que NÃO atravessa a fronteira: `dt`](#6-o-que-não-atravessa-a-fronteira-dt)
7. [Determinismo](#7-determinismo)
8. [Degradação: sem Python, a aeronave continua voando](#8-degradação-sem-python-a-aeronave-continua-voando)
9. [O que muda no `.edl`](#9-o-que-muda-no-epp)
10. [O que foi medido rodando](#10-o-que-foi-medido-rodando)
11. [Como verificar tudo](#11-como-verificar-tudo)

---

## 1. O que é novo, e o que não é

**Não é novo** — já existia e não foi tocado:

| peça | onde |
|---|---|
| o interpretador embarcado (`isAvailable`/`loadScript`/`decide`) | [`shared/xpyembed`](../../../shared/xpyembed/) |
| o nó de árvore `( PyDecide )` | `models/A4/src/bt/nodes/PyDecideAction.cpp` |
| a ordem canônica dos 28 campos | [`shared/xrlbridge/ObservationFields.hpp`](../../../shared/xrlbridge/ObservationFields.hpp) |
| a pilha inteira: `Aircraft` + `JSBSimModel` + `Autopilot` + radar + `AlertDatalink` + terreno | igual à das gêmeas |
| o plugin | o **mesmo** `libflight_tc.so` das gêmeas, byte a byte |

**É novo** — o que esta pasta acrescenta:

* um **subprojeto completo** em cima daquelas peças: cenário próprio, árvore própria, quatro
  scripts, portas próprias (Tacview **1237**, DIS **3004**), alvos de `make` próprios e a bateria
  de testes de sempre. Antes havia um `flight_tree_py.xml` de **exemplo** no `models/A4`, com
  um único nó e um script de dez linhas, exercitado só por um teste — não havia como *rodar* uma
  poc governada por Python e olhar para ela no Tacview.
* **quatro** folhas em Python no lugar de uma, uma por ramo da árvore de produção. É isso que faz
  o rótulo de comportamento continuar significando alguma coisa (`bt=PY-EVADE`, `bt=PY-SUPPORT`)
  em vez de um `bt=PY` constante.
* políticas que **fazem o trabalho inteiro** — patrulha, evasão, apoio e retorno à base — e não um
  exemplo de "vira para longe e acelera".

**Nenhuma linha de C++ foi escrita para isto.** O host é uma cópia do da `multi-thread` com
caminhos e banner trocados; o modelo não mudou. Isso é o resultado a observar, não uma economia:
a extensibilidade que o `( PluginModule )` + `( PyDecide )` prometiam se paga aqui.

---

## 2. A árvore: condição em C++, ação em Python

[`configs/flight_tree_python.xml`](configs/flight_tree_python.xml) tem a **mesma forma** da árvore
de produção — um `Fallback` com as quatro prioridades na mesma ordem. A troca está nas folhas de
**ação**:

| prioridade | condição (C++, do plugin) | ação (Python, editável) |
|---|---|---|
| 1) pouco combustível | `FuelLow margin="0.05"` | [`policy/rtb.py`](configs/policy/rtb.py) |
| 2) evasão valendo | `ContactDetected` | [`policy/evade.py`](configs/policy/evade.py) |
| 3) alerta recebido | `AlertReceived` | [`policy/support.py`](configs/policy/support.py) |
| 4) nada acontecendo | — | [`policy/patrol.py`](configs/policy/patrol.py) |

**Por que as condições continuam em C++.** `ContactDetected` não pergunta "estou vendo o intruso
agora": ele consulta `domain::ThreatPolicy::engaged()`, que continua verdadeiro por `evadeHold`
segundos **depois** de a pista sumir. Essa histerese envelhece com o `dt` do frame, e um script
não recebe `dt` (§6). Sem ela, a própria quebra tira o intruso do setor do radar (±30° contra uma
quebra de 110°), a pista pisca, e os ramos 2 e 3 — que comandam sentidos **opostos** sobre o
mesmo objeto — alternam para sempre. É a oscilação já documentada no cabeçalho do
`flight_tree.xml` de produção, e ela não desapareceria só por a manobra passar a ser escrita em
Python.

**Por que `( ReportAndEvade )` sobreviveu dentro do ramo 2.** Ele faz duas coisas: fixa o comando
de quebra **e** marca o pedido de alerta tático. O `( PyDecide )` logo depois sobrescreve o
comando — `FlightDecision::take()` troca rumo, altitude e rótulo — mas **não** limpa o pedido de
alerta. O resultado é o desejado, e é o que faz o ramo 3 existir de verdade aqui:

> quem **avisa** os outros é o C++; quem **manobra** é o Python.

---

## 3. Os quatro scripts

Cada um é autocontido: constantes no topo, `decide(obs)` no fim, e um cabeçalho dizendo o que ele
faz e **por que não é o que o C++ faz**.

| script | o que decide | diferença notável em relação ao nó C++ que ele substitui |
|---|---|---|
| `patrol.py` | a patrulha | o `( Patrol )` nativo voa **pernas cronometradas** e deriva; aqui a patrulha é **geométrica** — uma órbita de raio fixo em torno da base, com o rumo saindo da posição atual a cada tick, então qualquer desvio se corrige sozinho |
| `evade.py` | a quebra | mesma regra de `domain::ThreatPolicy::breakCommand()`, inclusive a parte difícil: o alvo é **fixado na entrada** da manobra, nunca recalculado a cada tick (§5) |
| `support.py` | ir ao ponto avisado | igual ao `( SupportAlert )`, mais um piso de terreno **local** — a altitude vem de outra aeronave, sobre outro ponto da serra |
| `rtb.py` | voltar para a base | igual ao `( ReturnToBase )`, com a altitude de cruzeiro *latcheada* em vez de escrita no `.edl` |

As duas funções de ângulo (`_wrap360`, `_rumo_para`) estão **repetidas** nos quatro arquivos, de
propósito: um `import` traria o **mesmo** objeto de módulo para as quatro aeronaves, porque
`sys.modules` é compartilhado e só o dicionário de globais é que não é (§5). São oito linhas de
matemática pura; a alternativa seria a única porta de estado compartilhado desta pasta.

---

## 4. O contrato: 28 floats entram, 3 saem

```python
def decide(obs) -> (heading_deg, altitude_m, speed_kts)
```

`obs` são 28 floats na ordem canônica de
[`shared/xrlbridge/ObservationFields.hpp`](../../../shared/xrlbridge/ObservationFields.hpp) — 23
floats e depois 5 booleanos, todos vindos do mesmo `domain::WorldView` que o UBF já usa para
decidir. Os três campos de texto (`contactName`, `alertSender`, `alertContactName`) ficam de fora:
não são números.

**Essa é exatamente a entrada de um `.onnx`.** Um script desta pasta pode virar uma política
treinada em [`src/rl`](../../rl/) sem tocar em mais nada — troca-se `( PyDecide script=... )` por
`( OnnxPolicy model=... )` na árvore e o resto continua igual. É por isso que a ordem é tratada
como contrato, mantido numa X-macro única, e não como detalhe.

---

## 5. Estado por aeronave, e por que ele é seguro

`shared/xpyembed` dá a **cada aeronave** o seu próprio dicionário de globais para cada script.
Duas consequências, e as duas são usadas aqui:

**(a) Uma variável global de módulo é estado _por aeronave_, e sobrevive entre ticks.** É assim
que `patrol.py` e `rtb.py` guardam a altitude de cruzeiro: cada falcon nasce numa altitude própria
(1750 a 2100 m, calculada no `.edl` contra o pico do circuito de cada um), e o script simplesmente
guarda a altitude da **primeira** decisão. Ele não precisa saber que existem quatro aeronaves nem
qual delas está rodando.

**(b) Dois aviões rodando o mesmo arquivo não se enxergam** — que é o que mantém o resultado
independente da ordem em que as threads do pool adquirem o GIL, e portanto o que mantém o
`make check-python-flight` verde.

**O caso difícil é o `evade.py`.** Recalcular o alvo da quebra a cada tick é a armadilha clássica:
`meu_rumo + 110°` devolvido 50 vezes por segundo é um *setpoint* que foge na mesma velocidade em
que a aeronave gira, e a curva nunca termina. O C++ resolve isso com um `if (!engaged_)` — ele tem
a máquina de estados na mão. Um script não tem: `decide()` é chamado sem contexto nenhum sobre o
que aconteceu antes.

O que ele **tem** é o dicionário de globais e a própria posição. Daí o detector de episódio: entre
dois ticks **consecutivos** a aeronave anda ~1,6 m (82 m/s a 50 Hz), enquanto entre dois episódios
de evasão ela passa minutos patrulhando. Um salto de posição maior que 50 m significa, portanto,
"faz tempo que este script não é chamado" — ou seja, uma quebra **nova**, que merece um alvo novo.
Sem relógio, sem contador de tempo, e determinístico.

**O que continua compartilhado** é o `sys.modules`: um `import` traz o mesmo objeto de módulo para
todas as aeronaves. Um módulo importado com estado mutável ainda é um buraco — é o limite
conhecido de `shared/xpyembed`, e é por isso que os helpers estão repetidos (§3).

---

## 6. O que NÃO atravessa a fronteira: `dt`

A fronteira de `shared/xpyembed` é deliberadamente estreita: **28 floats entram, 3 saem**. Não há
`dt`, não há acesso ao `Player`, não há como chamar de volta o modelo.

Isso não é uma limitação a contornar — é o que torna cada script **puro em relação ao estado da
própria aeronave**, que é a condição do determinismo (§7). O preço é que nada nesta pasta pode
integrar tempo, e é por isso que:

* a patrulha é geométrica (órbita), não cronometrada (pernas);
* o fim do episódio de evasão é detectado por **distância percorrida**, não por temporizador;
* a histerese que de fato precisa de `dt` — `domain::ThreatPolicy` — continua onde estava, em C++,
  atrás do `( ContactDetected )`.

---

## 7. Determinismo

`make check-python-flight` roda 2000 frames de passo fixo com **1, 2 e 4** threads de tempo
crítico, mais uma repetição com 4, e exige que os quatro dumps `frame=` saiam **byte-idênticos** —
e as mensagens do `shared/xmsg` também. Com quatro aeronaves chamando `decide()` **em paralelo**,
na fase 3, sobre **um** interpretador cujo GIL é adquirido em ordem arbitrária.

O que sustenta isso são as três propriedades acima, nesta ordem:

1. o isolamento de globais por aeronave (§5) — sem ele, a ordem do GIL apareceria no resultado;
2. a ausência de `dt` (§6) — sem ela, um script poderia integrar tempo de parede;
3. os scripts não lerem relógio, não sortearem sem semente e não escreverem arquivo.

O `-deterministic` também **desliga o `shared/xlog`**, então as linhas de transição de
comportamento (que carregam o número da thread) ficam fora do modo comparável.

---

## 8. Degradação: sem Python, a aeronave continua voando

Sem interpretador Python no sistema, script ausente, sem `decide()`, ou exceção: o
`( PyDecide )` devolve `FAILURE`. Os ramos 1 a 3 falham com ele (`Sequence`), o ramo 4 também, e o
`( Patrol )` **nativo** no fim do `Fallback` assume — configurado pelos slots `patrol*`/`legTime`
que o cenário já tem.

Medido, removendo `configs/policy/patrol.py` do lugar:

```
frame=100 player=falcon1 ... bt=PATROL ...        # o nó C++, não o script
```

A aeronave continua voando, na patrulha cronometrada de sempre. É a mesma política de degradação
do joystick ausente em [`shared/xjoystick`](../../../shared/xjoystick/): a peça opcional some, o
resto não.

---

## 9. O que muda no `.edl`

[`configs/scenario.edl.in`](configs/scenario.edl.in) é o da `multi-thread` com quatro deltas:

1. `treeFile:` aponta para a árvore desta pasta, e não para a de produção instalada em `dist/`;
2. Tacview na porta **1237** e gravação em `data/recordings/` próprio;
3. DIS emitindo da porta **3004** (bandit 3001, single-thread 3002, multi-thread 3003) — as
   quatro pocs podem rodar ao mesmo tempo;
4. o `MsgFileSink` grava no `data/messages/` próprio.

**Os números dos slots do `( BtBehavior )` deixaram de ser a política** — cada script carrega os
seus. O que esses slots ainda governam está anotado no próprio arquivo: `fuelReserve` alimenta o
`( FuelLow )`, os `evade*`/`breakTurn`/`terrainClearance` alimentam a histerese de
`domain::ThreatPolicy`, e os `patrol*`/`legTime` alimentam o `( Patrol )` de degradação. Ficam
inertes, de propósito, `rtbAltitude`, `rtbSpeed`, `arrivalRadius` e `supportSpeed` — esses papéis
agora são de `rtb.py` e `support.py`.

O `( AltitudeSafetyBehavior )` continua no `( UbfArbiter )`, com **voto 90** contra os 50 da
árvore: uma política em Python que faça besteira não derruba a aeronave no terreno. É a razão de
`patrol.py` e `rtb.py` manterem a folga de terreno em **850 m**, acima do `recoverClearance` de
800 m — mantendo-se acima da rede de segurança, a política nunca entra em disputa com ela.

---

## 10. O que foi medido rodando

| | |
|---|---|
| determinismo, 2000 frames, 1 / 2 / 4 threads T/C + repetição | **dumps byte-idênticos**, mensagens inclusive |
| cadeia entre players, na fixture com intruso | `PY-PATROL` → `PY-EVADE` em falcon1 → `PY-SUPPORT` nas outras três, com `sent`/`recv` batendo |
| as quatro aeronaves decidindo em paralelo | `thread 1`, `thread 2`, `thread 3`, `thread 4` no `data/logs/` — quatro `decide()` no mesmo frame |
| altitude de cruzeiro *latcheada* por aeronave | 1750 / 1850 / 2050 / 2100 m, exatamente as do `.edl`, sem o script saber quais são |
| precisão da órbita geométrica | raio mantido a **menos de 1 m** dos 5 NM nominais, sem deriva |
| degradação com o script removido | `bt=PATROL` (o nó nativo); a aeronave não cai nem congela |
| custo do Python no frame | **~42 µs por decisão**, ~0,8% de um frame de 20 ms (ver abaixo) |
| linhas de C++ escritas para tudo isto | **zero** (o host é cópia; o modelo não mudou) |

**O custo, medido.** Os dois binários rodando a **mesma** fixture com intruso, 4000 frames de
passo fixo, 4 threads T/C, três repetições cada:

| | tempo de parede |
|---|---|
| `multi-thread` (decisão em C++) | 2,36 / 2,62 / 2,60 s |
| `python-flight` (decisão em Python) | 3,01 / 3,34 / 3,25 s |

A diferença é ~0,67 s em **16 000 decisões** (4000 frames × 4 aeronaves) — **~42 µs por decisão**,
ou ~168 µs por frame contra um orçamento de 20 ms. O GIL serializa as quatro aeronaves, e ainda
assim sobra folga de duas ordens de grandeza. **Não faça I/O dentro de `decide()`** — é o único
jeito conhecido de gastar esse orçamento.

---

## 11. Como verificar tudo

```bash
make check-python-flight                    # determinismo: 1, 2 e 4 threads T/C
meson test -C build --suite scenario        # inclui scenario-{intruder,lowfuel,terrain}-python-flight
meson test -C build --suite memory          # inclui memory-python-flight
meson test -C build --suite determinism     # inclui determinism-python
```

As três fixtures de cenário são **derivadas** do `scenario.edl.in` desta pasta por
`tests/scenario/make_fixture.py` — nunca cópias versionadas. Elas rodam a **mesma** bateria
semântica das gêmeas (quem evadiu avisa, quem apoiou recebeu, ninguém voou para dentro do
terreno), normalizando o prefixo `PY-` na entrada: as propriedades afirmadas são as do **modelo**,
e valem igual quando o comando sai de um script.

Para ver o voo: `make run-python-flight` e conecte o Tacview em `File > Real-Time Telemetry`,
porta **1237**.
