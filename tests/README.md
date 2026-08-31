# `tests/` — a suíte automatizada

```bash
make configure                       # inclui gtest (test_requires no conanfile.py)
meson configure build -Dtests=true   # a suíte fica atrás desta opção
make build
make test                            # 13 testes, ~13 s
```

`-Dtests=true` existe para que um build comum não precise do gtest resolvido. Sem ele o
`subdir('./tests')` do [meson.build](../meson.build) raiz nem é avaliado.

Para rodar uma camada só: `meson test -C build --suite domain` (ou `tree`, `scenario`,
`memory`, `determinism`, `guard`).

---

## Por que cinco camadas

O repositório já tinha verificação automatizada — os `check-*` de determinismo — mas ela responde
uma pergunta estreita: *o estado é reprodutível?* Um modelo que decide **errado** passa nos
checks sem reclamar, desde que decida errado sempre igual. O que faltava era travar o
comportamento em si.

Cada camada responde uma pergunta diferente e custa uma ordem de grandeza a mais que a anterior:

| suite | pergunta | como | custo |
|---|---|---|---|
| `domain` | as regras estão certas? | GTest sobre `domain/` **e sobre `shared/xmsg/rules/`**, sem MIXR e sem BT.CPP | 66 testes, ~10 ms |
| `tree` | a máquina de estados está certa? | o `flight_tree.xml` **de produção** contra um contexto falso | 15 testes, ~10 ms |
| `scenario` | o modelo se comporta voando? | o binário de verdade, com fixture, asserções sobre `frame=` | 6 execuções |
| `memory` | vaza objeto? | contadores de instância do MIXR + o `states` do `msgHealth` | 4 execuções |
| `determinism` | é reprodutível, nos dois laços de decisão? | 1, 2 e 4 threads T/C, dump `frame=` **e** o `.jsonl` do `xmsg` | 8 execuções |
| `guard` | as duas pocs continuam gêmeas? | `diff -r` entre `domain/` e `bt/` | instantâneo |

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
[`domain/ThreatPolicy.hpp`](../src/single-thread/include/domain/ThreatPolicy.hpp): três correções
que vieram de ver as aeronaves "batendo asa" no Tacview. Cada uma virou um teste, porque cada uma
é uma regressão que voltaria em silêncio:

- o alvo da evasão é fixado **na entrada** da manobra, não recalculado a cada tick;
- o rumo de fuga sai da marcação **absoluta do contato**, não do próprio nariz;
- `engaged()` sobrevive ao contato por `holdSeconds` exatos — a histerese.

Mais o piso anti-CFIT, com **varredura de invariante**: para uma grade de elevação × altitude ×
marcação × sentido do contato, a altitude comandada nunca fica abaixo de `terreno + folga`. É
laço aninhado comum, sem dependência de *property testing*.

## Camada 2 — a árvore ([tree/](tree/))

Carrega o
[`flight_tree.xml` de produção](../src/single-thread/configs/flight_tree.xml) — por caminho, não
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
> 1. `FlightState::Snapshot` virou [`domain::WorldView`](../src/single-thread/include/domain/WorldView.hpp),
>    com `using Snapshot = domain::WorldView;` mantendo todos os call sites. A estrutura nunca teve
>    tipo do MIXR — o que a prendia ao framework era só morar dentro de uma classe que herda de
>    `AbstractState`.
> 2. `NodeContext` deixou de carregar um `BtBehavior*` concreto e passou a apontar para
>    [`bt_nodes::DecisionContext`](../src/single-thread/include/bt/DecisionContext.hpp), a interface
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
> para o mesmo `.acmi` — dois `ofstream` truncando a mesma gravação, em silêncio, nos 13 testes.
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
[`app/MetaObjectReport`](../src/single-thread/include/app/MetaObjectReport.hpp) imprime uma linha
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
[`BehaviorBoard`](../src/single-thread/include/xnative/BehaviorBoard.hpp), no ponto da atuação.

**A saída de mensagens entra na mesma comparação.** O `shared/xmsg` **não** é desligado em
`-deterministic` (ao contrário do `xlog`): tudo que ele emite carrega tempo simulado, nunca
relógio de parede nem id de thread. Então o `.jsonl` tem de ser byte-idêntico com 1, 2 e 4
threads, e isso é asserção — não precaução.

**Quando a diferença falha, a mensagem diz onde.** Em vez de "os arquivos diferem":

```
FALHA threads-1 != threads-2
      primeira divergencia: frame=600 player=falcon1  threads-1:bt=PATROL  threads-2:bt=SUPPORT
```

## Guarda — a duplicação ([guard/](guard/))

`domain/` e `bt/` são byte-idênticos entre as duas pocs. Isso era convenção implícita:
`compare-single-multi` mostra as diferenças, mas não falha, e ninguém lê a saída toda. Aqui vira
invariante verificado — e é ele que justifica as camadas 1 e 2 compilarem contra **uma cópia só**.
Se as duas divergissem, metade do modelo ficaria sem teste em silêncio.

---

## Armadilhas encontradas montando isto

**1. O `-deterministic` não é hermético com o cenário de produção.** O bloco `networks:` abre a
porta DIS 3000 e ingere PDUs de quem estiver na LAN. Com um `bandit-dis` de outra sessão no ar,
duas execuções idênticas divergem e o `check-single-thread` acusa **falso não-determinismo** —
medido: `frame=600 falcon1` deu `PATROL` com 1 thread e `SUPPORT` com 2, porque o intruso da rede
apareceu em uma e não na outra. Todas as fixtures removem `networks:`, e os alvos `check-*`
passaram a rodar hermético. Assim, as duas pocs passam com 1, 2 e 4 threads em 2000 frames.

**2. Os contadores de instância não são atômicos.** `++metaObject.count` é `int` cru
(`macros.hpp:247-255`); com os agentes decidindo em paralelo no pool T/C os incrementos correm
entre si. O teste de vazamento roda com `-threads 1`.

**3. Os testes que executam uma poc são `is_parallel: false`.** `app/ScenarioTemplate` grava o
cenário expandido sempre no mesmo caminho (`src/<poc>/configs/scenario.generated.epp`, não
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
