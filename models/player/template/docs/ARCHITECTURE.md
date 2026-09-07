# Arquitetura deste template

## O que este diretório é, e o que ele NÃO é

Isto **não é um modelo de produção** — não decide nada de interessante, não é carregado por
nenhum cenário existente, e não deveria estar no seu commit final (você vai copiar isto para uma
pasta com outro nome e apagar/reescrever quase tudo). É o **esqueleto mínimo, em camadas**, que
demonstra a forma que os modelos "de verdade" deste repositório usam, com uma decisão de exemplo
em cada camada — pequena o bastante para ler inteira em poucos minutos, real o bastante para
compilar, testar e carregar num cenário de verdade.

Se você só precisa da prova de que "o contrato de plugin basta" (sem nenhuma camada, sem
`domain/`), o ponto de partida certo é
[`../../fixtures/stub`](../../fixtures/stub/README.md) — leia
[`../../fixtures/stub/docs/CONTRATO.md`](../../fixtures/stub/docs/CONTRATO.md) primeiro. Este
diretório aqui existe para o caso oposto: você **vai** escrever um modelo com mais de uma
decisão, e quer começar já na forma que vai precisar mais cedo ou mais tarde. A tabela abaixo
resume as diferenças entre os quatro pontos de referência que este repositório tem hoje:

| projeto | por que existe | o que copiar dele |
|---|---|---|
| [`fixtures/stub`](../../fixtures/stub/) | prova que o contrato de plugin **basta** — nenhuma camada, um arquivo só | a lista de obrigações (`docs/CONTRATO.md`) |
| **`template`** (este) | ponto de partida **em camadas**, para decisão nova, sem BehaviorTree.CPP ainda | a separação `domain/`→`ubf/`→`xnative/`, o `meson.build`, o `Makefile` |
| [`missile`](../../missile/) | segundo modelo real: um `Player` **novo** (não um agente de decisão) | como anexar um `JSBSimModel`, como evitar a indireção de `xnative/factory.*` quando há só 1-2 classes |
| [`A4`](../../A4/) | o modelo de produção — árvore de comportamento completa, terreno, RL, ONNX, Python embarcado | qualquer coisa além do que as três referências acima já cobrem |

## As quatro camadas, e por que a separação existe

```
domain/    -- regras de negócio PURAS. Sem MIXR, sem BehaviorTree.CPP, sem
              ponteiro nenhum de framework. Testável com um `executable()`
              que nem linka o MIXR (ver tests/meson.build) -- é a camada
              mais barata de testar e a que muda com mais frequência
              durante o ajuste fino de um modelo.

ubf/       -- as três interfaces do UBF do MIXR (percepção, decisão, ação),
              implementadas contra ESTE modelo. É aqui que domain/ encontra
              o mundo do MIXR: ExampleState lê um Player e produz números
              crus; ExampleBehavior aplica uma regra de domain/ sobre esses
              números; ExampleAction escreve de volta no player (e no
              xboard -- ver a seção própria, abaixo).

xnative/   -- a cola de registro: a factory que o boundary do plugin chama
              (ver src/plugin.cpp). Só paga por si a partir de ~3 classes;
              um modelo de 1-2 classes (veja `missile/`) pode inline-ar a
              factory direto em plugin.cpp e pular este diretório.

(bt/)      -- NÃO existe neste template. É onde uma árvore de comportamento
              (BehaviorTree.CPP) entraria, se e quando UMA regra deixar de
              bastar -- ver "Quando isto não bastar mais", abaixo.
```

Cada camada é um projeto Meson à parte na sua "testabilidade": `domain/` não linka nada do MIXR
(nem em produção, nem em teste); `ubf/`+`xnative/` linkam o MIXR e o SDK, mas nunca levantam uma
`Station` de verdade. Isso não é acidente — é o que faz `make test` deste diretório rodar em
milissegundos, e é a mesma divisão que `models/player/A4/docs/ARCHITECTURE.md` explica com muito
mais detalhe (a distinção entre a suíte `domain`, a suíte `tree` e a suíte `native` do modelo de
produção é exatamente esta, escalada).

## Por que `domain::` mora DENTRO de `mixr::models::xtemplate`, e não solto

Repare que `include/domain/ExampleThreshold.hpp` **não** declara um `namespace domain { ... }`
solto no escopo global — ele aninha em `mixr::models::xtemplate::domain`. Isto é deliberado, e o
comentário completo está no próprio header: um cenário pode carregar **mais de um** plugin no
mesmo processo (por exemplo, o seu modelo ao lado de `flight`/`missile` num cenário de demo), e
dois tipos com o **mesmo nome qualificado** (`domain::Foo`) em dois `.so`s distintos têm o mesmo
símbolo *mangled* — a comparação de `type_info` deste toolchain degrada para `strcmp` entre
objetos `RTLD_LOCAL`, então dois tipos DIFERENTES com o mesmo nome qualificado podem colidir. O
`flight` (`models/player/A4`) chegou primeiro e usa `domain::` solto — já documentado e usado em
dezenas de lugares, caro demais para mudar agora. O `missile` (mais novo) já nasceu com
`domain::` aninhado sob `xmissile::`. Este template segue a convenção mais nova: ao copiá-lo,
troque `xtemplate` pelo nome do seu modelo em TODA a árvore (ver
[`PRIMEIROS-PASSOS.md`](PRIMEIROS-PASSOS.md)), e o seu `domain::` sai automaticamente livre de
colisão com qualquer outro plugin.

## A obrigação que falha em silêncio: escrever no `xboard`

`ubf::ExampleAction::execute()` termina com duas chamadas:

```cpp
xboard::setBehaviorLabel(player->getID(), label);
xboard::bumpDecisionCount(player->getID());
```

Isto **não é exigido pelo compilador, nem pelo carregador de plugin** — um modelo que nunca
chame essas funções compila, carrega, satisfaz `provides:`, e o host sobe e roda. A única
diferença observável é que a tela de status e o dump `-deterministic` mostram `bt=--` e `dec=0`
**para sempre**, sem nenhum erro em lugar nenhum. É a obrigação mais fácil de esquecer porque é a
única sem sintoma de falha — leia
[`../../fixtures/stub/docs/CONTRATO.md`](../../fixtures/stub/docs/CONTRATO.md) seção 3 para a lista
completa de funções do `xboard` (alerta tático, contadores de datalink, varredura de radar,
thread de decisão) e quando cada uma se aplica ao SEU modelo.

## O que este template NÃO demonstra, de propósito

- **Slots com todas as unidades do MIXR** — `ExampleBehavior` só usa `base::Distance`. Se o seu
  modelo precisa de ângulos, tempos, velocidades, etc., `models/player/A4/include/ubf/BtBehavior.hpp`
  e `models/player/fixtures/stub/src/stub.cpp` têm exemplos de cada um.
- **Comandar um subsistema de verdade** — `ExampleAction::execute()` só escreve no `xboard`, para
  compilar contra qualquer `Player`, não só aeronaves. Substitua o corpo por chamadas a
  `models::Autopilot`/`models::StoresMgr`/o que for relevante para o SEU player — veja
  `models/player/A4/src/ubf/FlightAction.cpp` para o padrão completo (incluindo o log de
  transição de estado via `libs/xlog`, que também foi deixado de fora daqui por simplicidade).
- **Dados próprios do modelo** (uma árvore XML, um `.onnx`, uma aeronave JSBSim) — nenhum
  `install_data()`/`install_subdir()` no `meson.build`. Se o seu modelo precisar de um arquivo
  próprio, publique-o do mesmo jeito que `models/player/A4/meson.build` publica `flight_tree.xml` e
  `data/jsbsim/` — a seção "O `.so` sozinho pode não ser a entrega completa" de
  `CONTRATO.md` cobre o "porquê".
- **Terreno, RL, ONNX, Python embarcado** — todos existem em `models/player/A4` como camadas
  adicionais opcionais, nenhuma delas é parte do "mínimo para um modelo funcionar".

## Quando isto não bastar mais

Este template decide com **uma regra só** (`ExampleThreshold`, um Schmitt trigger). O dia em que
o seu modelo precisar de mais de uma decisão coordenada — "se combustível baixo, RTB; senão, se
há contato, evade; senão, patrulha" — é o dia de trocar `ExampleBehavior::genAction()` por uma
árvore do BehaviorTree.CPP, exatamente como `models/player/A4/include/ubf/BtBehavior.hpp` faz.
Isso significa:

1. Adicionar `behavior_tree_dep = dependency('behaviortree.cpp.asa', method: 'pkg-config',
   required: true)` ao `meson.build` e colocá-la em `model_deps`/`model_link_args` (a
   `-Wl,--exclude-libs,ALL` já está lá, mas ela só importa a partir do momento em que você linka
   uma biblioteca **estática** — o que a BehaviorTree.CPP é, ver `models/README.md` seção 2.1).
2. Criar um diretório `bt/nodes/` com um nó por decisão (condição ou ação), registrados numa
   `BT::BehaviorTreeFactory` própria — `models/player/A4/src/bt/bt_factory.cpp` é a referência.
3. Trocar o corpo de `ExampleBehavior::genAction()` por um `tree.tickRoot()` sobre um
   `BT::Tree` carregado de um XML — `models/player/A4/src/ubf/BtBehavior.cpp` é a referência
   completa, incluindo o cache de árvore por caminho de arquivo (`g_treeBuildMutex`) que evita
   reparsear o XML a cada player.

Nenhuma dessas mudanças toca `domain/` nem `xnative/factory.*` — é exatamente a fronteira que a
separação em camadas existe para proteger.

## Ler também

- [`PRIMEIROS-PASSOS.md`](PRIMEIROS-PASSOS.md) — o roteiro mecânico de copiar isto e transformar
  num modelo com nome próprio
- [`../README.md`](../README.md) — como compilar, testar e instalar este diretório sozinho
- [`../../../README.md`](../../../README.md) — visão geral de `models/`, o contrato de plugin, e o
  build orquestrado pelo Makefile da raiz
- [`../../fixtures/stub/docs/CONTRATO.md`](../../fixtures/stub/docs/CONTRATO.md) — a lista completa e
  autoritativa do que um modelo precisa fazer (este documento resume só as partes relevantes à
  arquitetura em camadas)
