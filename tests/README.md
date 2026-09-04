# `tests/` — a suíte automatizada

```bash
make configure                       # inclui gtest (test_requires no conanfile.py)
meson configure build -Dtests=true   # a suíte fica atrás desta opção
make build
make test                            # 45 testes nas DUAS suítes
```

`-Dtests=true` existe para que um build comum não precise do gtest resolvido. Sem ele o
`subdir('./tests')` do [meson.build](../meson.build) raiz nem é avaliado.

**São DUAS suítes, em dois diretórios de build**, porque o modelo saiu para um projeto próprio
(`models/flight/`):

```bash
make test          # as duas
make test-models   # só a do modelo:  domain (42) + tree (15) + native (9)
meson test -C build --suite plugin   # só uma camada do host
```

> **Cuidado, e isto foi medido:** `meson test` devolve **rc=0 para suíte vazia** ("No tests
> defined."), e o default de `-Dtests` é `false`. `meson test -C build --suite domain` continua
> existindo, mas hoje ele roda só as primitivas do `shared/xmsg` — as regras do modelo estão em
> `build-flight`. Perder uma das duas suítes seria um verde silencioso, então `make test` e
> `make test-models` conferem a contagem com `meson introspect --tests` **antes** de rodar.

> **A terceira poc na bateria de cenário/memória/determinismo.** `src/poc/python-flight` troca as
> folhas de **ação** da árvore por scripts Python (`( PyDecide )`), rotulados `PY-<modo>`. Ela roda
> a **mesma** bateria semântica das gêmeas: o runner ganhou `--label-prefix`, que normaliza
> `PY-EVADE` → `EVADE` na entrada, em vez de uma cópia do script que envelheceria em silêncio ao
> lado dele. As propriedades afirmadas são as do **modelo** — quem evadiu avisa, quem apoiou
> recebeu, ninguém voou para dentro do terreno — e valem igual quando o comando sai de um `.py`.
> O `determinism-python` é o que prova que quatro `decide()` em paralelo, sobre **um** GIL
> adquirido em ordem arbitrária, ainda dão dumps byte-idênticos com 1, 2 e 4 threads T/C.

---

## Duas suítes, em dois projetos

O repositório já tinha verificação automatizada — os `check-*` de determinismo — mas ela responde
uma pergunta estreita: *o estado é reprodutível?* Um modelo que decide **errado** passa nos
checks sem reclamar, desde que decida errado sempre igual. O que faltava era travar o
comportamento em si.

Cada camada responde uma pergunta diferente e custa uma ordem de grandeza a mais que a anterior:

| suite | pergunta | como | custo |
|---|---|---|---|
| `domain` (modelo) | as regras estão certas? | GTest sobre `models/flight/src/domain/`, sem MIXR e sem BT.CPP | 42 testes, ~10 ms |
| `domain` (host) | as primitivas do `shared/xmsg` estão certas? | GTest sobre `shared/xmsg/rules/` | 24 testes, ~10 ms |
| `tree` (modelo) | a máquina de estados está certa? | o `flight_tree.xml` **de produção** contra um contexto falso | 15 testes, ~10 ms |
| `native` (modelo) | as classes MIXR próprias estão certas? | fábrica, tabelas de slot (tipo **e unidade**) e a fronteira de fase do datalink — **sem levantar Station** | 9 testes, ~10 ms |
| `scenario` | o modelo se comporta voando? | o binário de verdade, com fixture, asserções sobre `frame=` | 9 execuções |
| `memory` | vaza objeto? | contadores de instância do MIXR + o `states` do `msgHealth` | 6 execuções |
| `determinism` | é reprodutível, nos dois laços de decisão **e com a política escrita em Python**? **E de onde vem essa reprodutibilidade?** | 1, 2 e 4 threads T/C, dump `frame=` **e** o `.jsonl` do `xmsg`; mais o controle negativo `onde-a-decisao-roda` | 12 execuções + ~10 |
| `plugin` | a carga dinâmica cumpre o contrato, falha legivelmente e **funciona com um modelo desconhecido**? | contrato, guarda de símbolo, 7 modos de falha, *hot-swap* e o **stub** | 5 testes, ~3 s |
| `guard` | as pocs continuam gêmeas, o host continua **opaco** ao modelo, o `.so` está **fresco** e todo modelo tem as cinco peças? | `diff -r` da camada de aplicação + as guardas estruturais | instantâneo |

---

## Camada 1 — `domain/` ([domain/](domain/))

`domain/` nunca dependeu de MIXR nem de BehaviorTree.CPP, então já era testável — só não estava
sendo testado. Cinco arquivos, um por regra.

Desde o `shared/xmsg`, esta camada cobre também as quatro primitivas de detecção que o MIXR não
tem — limiar com histerese (`Schmitt`), deadband com memória (`Deadband`), derivada em janela
(`RateWindow`) e o piso de emissão que **adia em vez de descartar** (`EmitGate`). Foi aqui que
apareceu a armadilha de acumulador de tempo: `0.1` somado 10 vezes fica **abaixo** de 1,0 e `0.02`
somado 50 vezes fica **acima**, então um `hold: ( Seconds 1 )` armava em passos diferentes a
10 Hz e a 50 Hz. Dois testes vermelhos acharam isso antes de qualquer linha de MIXR ser escrita.

O que se trava aqui é, sobretudo, a história registrada no cabeçalho de
[`domain/ThreatPolicy.hpp`](../models/flight/include/domain/ThreatPolicy.hpp): três correções
que vieram de ver as aeronaves "batendo asa" no Tacview. Cada uma virou um teste, porque cada uma
é uma regressão que voltaria em silêncio:

- o alvo da evasão é fixado **na entrada** da manobra, não recalculado a cada tick;
- o rumo de fuga sai da marcação **absoluta do contato**, não do próprio nariz;
- `engaged()` sobrevive ao contato por `holdSeconds` exatos — a histerese.

Mais o piso anti-CFIT, com **varredura de invariante**: para uma grade de elevação × altitude ×
marcação × sentido do contato, a altitude comandada nunca fica abaixo de `terreno + folga`. É
laço aninhado comum, sem dependência de *property testing*.

## Camada 2 — a árvore ([tree/](../models/flight/tests/tree/))

Carrega o
[`flight_tree.xml` de produção](../models/flight/configs/flight_tree.xml) — por caminho, não
uma cópia. Um teste contra uma cópia provaria que a cópia está certa, o que não interessa a
ninguém.

Trava a **prioridade** do `Fallback`, que é a propriedade que nenhum nó isolado responde:
combustível baixo vence contato, contato vence alerta, alerta vence patrulha. E os dois rótulos
que os `check-*` não distinguem: `EVADE` (com pista) contra `BREAK` (no arrasto da histerese), com
a regra de que só se retransmite alerta enquanto se está **vendo** o intruso.

Um dos testes existe só para pegar o erro mais chato: nó novo no XML sem `registerBuilder` —
hoje isso não quebra o build, quebra o voo.

> **Esta camada só é possível por causa de duas mudanças no código de produção**, ambas mecânicas
> e provadas neutras (o dump determinístico saiu byte a byte idêntico ao de antes):
>
> 1. `FlightState::Snapshot` virou [`domain::WorldView`](../models/flight/include/domain/WorldView.hpp),
>    com `using Snapshot = domain::WorldView;` mantendo todos os call sites. A estrutura nunca teve
>    tipo do MIXR — o que a prendia ao framework era só morar dentro de uma classe que herda de
>    `AbstractState`.
> 2. `NodeContext` deixou de carregar um `BtBehavior*` concreto e passou a apontar para
>    [`bt_nodes::DecisionContext`](../models/flight/include/bt/DecisionContext.hpp), a interface
>    com os 8 getters que os nós já usavam. `BtBehavior` a implementa sem um método novo.
>
> Resultado: `ldd` no binário desta camada mostra **zero** bibliotecas do MIXR. O comentário de
> `NodeContext.hpp` já prometia "ou a um teste unitário sem simulação nenhuma" — isto cumpre.

## Camada 3 — cenário ([scenario/](scenario/))

Roda o binário com `-f <fixture> -deterministic N` e afirma **comportamento** sobre as linhas
`frame=`, não igualdade byte a byte. Um *golden* exato quebraria por ruído de ponto flutuante
entre máquinas e diria apenas "os arquivos diferem"; aqui a falha diz qual propriedade caiu.

Três modos, um por ramo da árvore:

| modo | força | como |
|---|---|---|
| `intruder` | a cadeia inteira: detecta → evade → avisa → os outros apoiam | reintroduz um `bandit1:` **local** |
| `lowfuel` | o ramo de RTB vence tudo | sobe `fuelReserve` acima do combustível real, pelo slot |
| `terrain` | `AltitudeSafetyBehavior` (voto 90) ganha do `BtBehavior` (voto 50) | sobe `minAltitude` acima da altitude de cruzeiro |

> **Correção de um bug latente, feita aqui:** `make_fixture.py` reescrevia `fileName:` com um
> `re.sub` **global**, e isso só funcionava porque havia exatamente **um** `fileName:` em cada
> cenário. Quando o `shared/xmsg` acrescentou o segundo (o `.jsonl`), os dois passariam a apontar
> para o mesmo `.acmi` — dois `ofstream` truncando a mesma gravação, em silêncio, ao longo da suíte.
> As substituições agora são ancoradas na extensão (`\.acmi"` e `\.jsonl"`).

As fixtures são **derivadas** do cenário de produção por
[`scenario/make_fixture.py`](scenario/make_fixture.py), e não cópias versionadas: uma cópia
começaria correta e envelheceria em silêncio — mexer no `scenario.epp.in` não quebraria teste
nenhum, que é o oposto do que se quer.

## Camada 4 — vazamento ([memory/](memory/))

Usa instrumentação que **já existia no MIXR e estava sem uso aqui**. Toda classe com
`DECLARE_SUBCLASS` carrega um `base::MetaObject` estático com três contadores públicos, mantidos
pelas macros `STANDARD_CONSTRUCTOR`/`STANDARD_DESTRUCTOR`:

| campo | é |
|---|---|
| `count` | instâncias **vivas** agora |
| `mc` | pico de instâncias simultâneas |
| `tc` | total já criado desde o início do processo |

O próprio `Object.hpp` diz para que serve: *"to spot potential memory leaks"*.
[`app/MetaObjectReport`](../src/poc/dis/single-thread/include/app/MetaObjectReport.hpp) imprime uma linha
`meta=` por classe vigiada ao fim de uma corrida determinística.

**A asserção é comparativa, e é isso que a torna válida.** Um retrato único não distingue
"vazando" de "retido de propósito": o teste roda 500 **e** 1000 frames e exige `count` igual entre
as duas, `tc` crescendo na proporção dos frames, e `mc` pequeno. Sem a segunda metade o teste
passaria por inércia — uma classe nunca instanciada tem `count` estável em zero para sempre.

Leitura saudável, medida:

```
meta=FlightAction count=0 mc=1 tc=4000     # 4 aviões × 1000 frames, nenhuma viva no fim
meta=FlightState  count=4 mc=4 tc=4        # criada uma vez por avião, no parse do EDL
```

### ASan é complementar, não redundante

Os contadores pegam vazamento de **ref-counting** (objeto vivo que ninguém mais alcança); o
LeakSanitizer pega `new` sem `delete` cru, que os contadores não enxergam. `make test-asan`
reconfigura com ASan, roda 500 frames e reverte.

Ele acusa **896 bytes em 22 alocações que não são do modelo** — todas de partida, dentro do
framework: `JSBSimModel::setSlotRootDir`/`setSlotModel` clonam uma `base::String` de slot e nunca
a liberam, mais `PrintHandler::setFullFilename` e `DataRecorder::setSlotEventName`. Nenhuma cresce
com os frames. Sem as supressões de [`memory/asan.supp`](memory/asan.supp) o alvo nasce vermelho e
vira ruído que ninguém volta a olhar; com elas, fica vermelho se **o nosso** código passar a
vazar. Se o fork for corrigido um dia, apaga-se a linha e o teste passa a cobrir mais.

## Camada 5 — determinismo ([determinism/](determinism/))

É a lógica que vivia inline no `Makefile`, extraída para
[`determinism/check_determinism.sh`](determinism/check_determinism.sh) — mesmas 4 execuções, mesmas
3 comparações — e registrada como **dois casos nomeados por onde a decisão roda**:

| caso | laço | agente |
|---|---|---|
| `determinism-nao-critico` | background, `updateData()` | `( SimAgent )` nativo, componente da `Station` |
| `determinism-critico` | fase 3 do frame de tempo crítico | `( FlightAgentTC )` próprio, componente do `Player` |

Duas coisas mudaram além da extração:

**1. A contagem de decisões virou asserção.** O `Makefile` imprimia o `dec=` da `falcon1` e não
verificava nada. Agora se afirma que `dec` avança na **mesma taxa** que `frame` entre dumps
consecutivos. A asserção *não* é `dec == frames`: a `multi-thread` decide uma vez a mais na
inicialização (601 em 600 frames, idêntico nas três configurações de thread) — isso é *offset* de
partida, não perda de vínculo com o frame. Comparar deltas mede a propriedade certa e ignora o
*offset*.

**2. A `single-thread` ganhou o campo `dec=`,** que antes só existia na `multi-thread`. Como ali a
decisão roda no laço de background, quem conta é o
[`BehaviorBoard`](../shared/xboard/Board.hpp), no ponto da atuação.

**A saída de mensagens entra na mesma comparação.** O `shared/xmsg` **não** é desligado em
`-deterministic` (ao contrário do `xlog`): tudo que ele emite carrega tempo simulado, nunca
relógio de parede nem id de thread. Então o `.jsonl` tem de ser byte-idêntico com 1, 2 e 4
threads, e isso é asserção — não precaução.

**Quando a diferença falha, a mensagem diz onde.** Em vez de "os arquivos diferem":

```
FALHA threads-1 != threads-2
      primeira divergencia: frame=600 player=falcon1  threads-1:bt=PATROL  threads-2:bt=SUPPORT
```


## Camada 6 — o modelo como plugin ([plugin/](plugin/))

`domain/`, `bt/`, `ubf/` e `xnative/` **não estão dentro do executável**: são um `shared_module`
aberto com `dlopen` durante o parse do cenário. Esta camada prova que o contrato é cumprido, que
as falhas são legíveis, e que o comportamento vem mesmo do `.so`.

| teste | o que prova |
|---|---|
| `plugin-contrato` | o descritor bate com o host; as 6 classes que o cenário nomeia são construíveis; `dynamic_cast` para `AbstractBehavior`/`AbstractState`/`Datalink` atravessa a fronteira do `.so`; slot próprio (`treeFile`) **e herdado** (`vote`) resolvem |
| `plugin-simbolo` | `nm -D` mostra `mixr_plugin_v1` como `T` — **e nada mais**, apesar dos 35 MB de BehaviorTree.CPP **estática** linkados dentro (é o que valida o `-Wl,--exclude-libs,ALL`) |
| `plugin-negativos` | 7 modos de falha, **cada um afirmando `rc != 139`** |
| `plugin-hotswap` | mesmo binário + `.so` diferente = **voo** diferente: duas variantes do modelo com o sentido da curva de patrulha invertido, e o rumo do falcon1 divergindo ~107° |

**`plugin-contrato` é um gate, não só um teste.** Ele afirma que RTTI atravessa o `.so` com o
plugin compilado em `-fvisibility=hidden` e aberto com `RTLD_LOCAL`. Se ficar vermelho, a
resposta é tirar `gnu_symbol_visibility: 'hidden'` do `meson.build` do plugin — o `RTLD_LOCAL`
sozinho já cobre o essencial.

**Por que todo caso negativo afirma `rc != 139`.** Antes desta camada, dois deles terminavam em
SIGSEGV mudo, e a causa era do próprio `edl_parser`: a mensagem `"undefined factory name"`
(`edl_parser.y:97-100`) está num ramo inalcançável, e `slot_value : SLOT_ID form`
(`edl_parser.y:179`) faz `$2->unref()` sem checar nulo. Se alguém remover a checagem de nome
desconhecido de `mixrFactory()`, estes testes ficam vermelhos em vez de o repositório voltar a
estourar em silêncio.

**Cobertura de graça.** Como o `( PluginLoader )` vive no `.epp.in` de produção e as fixtures são
**derivadas** dele, as camadas 3, 4 e 5 passaram a exercitar o plugin sem uma linha nova nos
scripts.

## Guarda — a duplicação ([guard/](guard/))

`domain/` e `bt/` são byte-idênticos entre as duas pocs. Isso era convenção implícita:
`compare-single-multi` mostra as diferenças, mas não falha, e ninguém lê a saída toda. Aqui vira
invariante verificado — e é ele que justifica as camadas 1 e 2 compilarem contra **uma cópia só**.
Se as duas divergissem, metade do modelo ficaria sem teste em silêncio.

### `modelo-estrutura` — as cinco peças de todo projeto de modelo

[`guard/check_modelo_estrutura.sh`](guard/check_modelo_estrutura.sh) afirma que **todo** projeto
sob `models/` tem `tests/`, `docs/`, `README.md`, `CHANGELOG.md` e `Makefile` — a regra escrita em
[models/README.md](../models/README.md), que antes só existia como prosa. É o que faz cada modelo
autocontido: quem abre o editor só em `models/<nome>/` tem ali como compilar, como provar que
continua certo, o "porquê" das decisões, a porta de entrada e o que mudou desde a última vez.

Ela **descobre os projetos por `find`** (todo diretório com `project()` no `meson.build`), não por
lista fixa — mesma lição já registrada no cabeçalho de `check_host_opaco.sh`, onde um glob de dois
níveis passou a mentir em silêncio depois de uma renomeação. Modelo novo já nasce cobrado, e
`plugins/` fica de fora de propósito: é o depósito de `.so` de terceiro, não um projeto.
Diretório presente mas vazio (ou só com `.gitkeep`) conta como ausente.

---

## Armadilhas encontradas montando isto

**1. O `-deterministic` não é hermético com o cenário de produção.** O bloco `networks:` abre a
porta DIS 3000 e ingere PDUs de quem estiver na LAN. Com um `bandit` de outra sessão no ar,
duas execuções idênticas divergem e o `check-single-thread` acusa **falso não-determinismo** —
medido: `frame=600 falcon1` deu `PATROL` com 1 thread e `SUPPORT` com 2, porque o intruso da rede
apareceu em uma e não na outra. Todas as fixtures removem `networks:`, e os alvos `check-*`
passaram a rodar hermético. Assim, as duas pocs passam com 1, 2 e 4 threads em 2000 frames.

**2. Os contadores de instância não são atômicos.** `++metaObject.count` é `int` cru
(`macros.hpp:247-255`); com os agentes decidindo em paralelo no pool T/C os incrementos correm
entre si. O teste de vazamento roda com `-threads 1`.

**3. Os testes que executam uma poc são `is_parallel: false`.** `app/ScenarioTemplate` grava o
cenário expandido sempre no mesmo caminho (`src/poc/<poc>/configs/scenario.generated.epp`, não
configurável por linha de comando), então duas execuções simultâneas disputam o arquivo.

**4. `wrap180(180) == -180`.** O header documenta `(-180, 180]`, mas `fmod(360,360)==0` faz a
borda cair em -180. Não é inofensivo: `ThreatPolicy` escolhe o lado da quebra por
`relBearingDeg >= 0`, então um contato exatamente a ré quebra sempre para o mesmo lado. O teste
trava o comportamento **observado**, não o comentário — corrigir seria mudança de modelo.

**5. Comentário de fixture tem de ser ASCII puro**, como todo `.epp`: um único acentuado faz o
`edl_parser` recusar o arquivo inteiro com "syntax error", apontando a linha certa sem dizer o
motivo. `make_fixture.py` valida antes de gravar, para a falha vir com a causa na mão.

---

## Um teste que só passa não vale nada até falhar de propósito

As três camadas novas foram validadas invertendo o alvo. Cada quebra foi pega pela camada certa,
sem falso positivo nas outras:

| quebra deliberada | quem pegou |
|---|---|
| alvo da evasão recalculado a cada tick | `domain` — 1 teste (`AlvoEhFixadoNaEntradaENaoSegueOContato`) |
| `ContactDetected` volta a ser "estou vendo agora", sem histerese | `tree` — 2 testes |
| `ref()` a mais na `FlightAction` | `memory` — `count` 2000 → 4000, proporcional aos frames |

---

## O controle negativo: de onde vem o determinismo

`make check-single-thread` prova que a poc é reprodutível. Ele **não** prova por quê — e a
resposta natural ("porque roda em passo fixo") está errada.

`tests/determinism/check_onde_a_decisao_roda.py` separa as duas coisas. Ele roda as duas pocs com
`-parallel-decision`, que solta o laço de background numa thread própria, sem sincronizar com o
frame — exatamente o que o tempo real faz, só que sem o relógio de parede — e afirma:

| poc | onde a decisão roda | exigência |
|---|---|---|
| `single-thread` | `( SimAgent )`, laço de background | **tem de DIVERGIR** |
| `multi-thread` | `( FlightAgentTC )`, **fase 3** do frame, dentro da barreira | **NÃO pode divergir** |

Medido: a `single-thread` produziu **5 resultados distintos em 5 execuções**; a `multi-thread`, um
só. No cenário de patrulha a divergência é na **trajetória** (`n=9224.47` / `9226.59` / `9226.32`),
não só num contador.

**Concorrência sozinha não basta — e isso foi medido montando o teste.** Uma primeira versão deixou
`updateData()` concorrente com o `tcFrame()` mas manteve **uma decisão por frame**: as duas pocs
continuaram byte-idênticas em 5 execuções. As decisões deste modelo são todas por **limiar**, e os
comandos vêm do plano de voo, não do estado instantâneo — ler a posição alguns centímetros adiante
não muda nada. O que quebra é a **contagem** de decisões variar, porque aí o `PatrolPlan::advance()`
integra tempo diferente a cada execução.

Sem o lado da `multi-thread`, o teste só mostraria que concorrência quebra coisas. Sem o lado da
`single-thread`, o `check-*` poderia estar passando por inércia. É o par que prova a afirmação.
