# events/ — o eixo de eventos deste projeto

> Esta pasta é onde a convenção abaixo é documentada **e** onde o código de verdade (payload +
> token) mora. É uma `shared_library()` de fronteira de plugin como `shared/xboard`/`xlog`/
> `xtrack`/`xrlbridge`/`xinfer`/`xpyembed`, só que na RAIZ do repositório, em vez de dentro de
> `shared/x<nome>/` — exceção deliberada: evento é o eixo central da modelagem deste projeto,
> então ganhou pasta própria em destaque em vez de ficar mais um `shared/x<nome>` perdido entre
> outros. O **mecanismo** de publicação é idêntico ao das outras seis: `subdir('./events')` no
> `meson.build` raiz, publicada em `dist/lib/`+`dist/include/events/` pelo mesmo
> `poc-mixr-sdk.pc` — só o endereço do fonte mudou, nada do jeito como um modelo a consome.

## Como a pasta é organizada

```
events/
├── README.md                    este arquivo — a convenção
├── CHANGELOG.md                  uma entrada por evento adicionado/mudado
├── meson.build                    a shared_library(), agrega payloads/**/*.cpp
├── EventTokens.hpp                LEDGER ÚNICO de tokens — todo evento novo soma UMA linha aqui
└── payloads/
    └── EID_ALERT/                UMA PASTA POR EVENTO, nomeada IGUAL ao token
        ├── TacticalAlert.hpp
        └── TacticalAlert.cpp
```

Duas coisas crescem aqui, em ritmos diferentes, e por isso moram separadas:

- **`EventTokens.hpp` fica único, de propósito.** O valor inteiro dele está em ser o ÚNICO lugar
  onde dois eventos poderiam colidir de número — espalhar a alocação por vários arquivos
  destruiria essa garantia bem no momento em que ela mais importa (a pasta crescendo). Ele cresce
  em **linhas**, não em estrutura: se um dia isso virar um problema de verdade (dezenas de
  tokens, categorias claras emergindo), a resposta é agrupar em blocos comentados dentro do
  próprio arquivo, não quebrá-lo em vários — continua sendo UM grep, nunca uma busca em N
  lugares.
- **`payloads/<TOKEN>/` é onde todo evento novo ganha a própria pasta**, nomeada EXATAMENTE igual
  à constante do token (`payloads/EID_ALERT/` para `events::EID_ALERT`) — quem lê o nome do token
  acha o payload correspondente só por isso, sem precisar grepar. Dentro, o par `.hpp`/`.cpp` de
  sempre; se o evento precisar de mais arquivos (helpers, sub-tipos), eles entram na MESMA pasta,
  sem afetar os outros eventos. Uma pasta por evento desde o primeiro, em vez de esperar
  "crescer demais" — o índice de navegação (nome da pasta = nome do token) já vale com um evento
  só, e não exige reorganizar nada quando o segundo, terceiro, enésimo evento chegar.

Nenhuma categoria temática por cima disso (ex.: agrupar `EID_ALERT`/`EID_EXPLOSION` num
`payloads/combat/`) — se uma taxonomia clara emergir com o tempo, ela pode nascer como mais um
nível (`payloads/combat/EID_EXPLOSION/`), mas isso é decisão para quando houver eventos de sobra
para agrupar, não antes.

## O que é um "evento" aqui

O MIXR **não tem** broker, fila global, pub/sub nem roteamento declarativo no `.epp` — e essa
ausência é deliberada, documentada em
[`src/poc/single-thread/README.md` §9](../src/poc/single-thread/README.md#9-interação-entre-players).
A única primitiva de interação entre objetos no MIXR inteiro é:

```cpp
virtual bool mixr::base::Component::event(const int token, base::Object* const obj = nullptr);
```

Síncrona, chamada na thread de quem emite, sobre um destinatário achado varrendo
`WorldModel::getPlayers()` — exatamente como radar, datalink, colisão, IR e kill nativos já
funcionam. Este documento **não propõe um mecanismo novo**: formaliza como já se usa essa mesma
primitiva de um jeito reaproveitável, com dois passos deliberadamente desacoplados.

## Os dois passos

1. **Definir o evento** — uma constante `int` (o token, em [EventTokens.hpp](EventTokens.hpp)) +
   uma classe de payload (`mixr::base::Object`-derivada, com
   `DECLARE_SUBCLASS`/`IMPLEMENT_SUBCLASS`, como qualquer objeto MIXR ref-contado — ex.:
   [payloads/EID_ALERT/TacticalAlert.hpp](payloads/EID_ALERT/TacticalAlert.hpp)). Isso é a "interface": o formato dos dados, nada de
   comportamento.
2. **Tratar o evento** — em qualquer classe pertinente, escrita **depois** e sem nenhuma relação
   de compilação com quem emite: um bloco `BEGIN_EVENT_HANDLER`/`ON_EVENT_OBJ`/
   `END_EVENT_HANDLER` (`mixr/base/macros.hpp`) que expande um override de `event()`. **Não há
   lista central de "quem escuta o quê"** — cada classe assina um token só por escrever o próprio
   `ON_EVENT_OBJ`. Adicionar um handler novo nunca exige editar o evento nem nenhuma outra classe.

## As duas formas de despacho já em uso

| forma | quando usar | exemplo |
|---|---|---|
| **(a) Subsistema nativo** — reaproveitar um hook já exposto por uma classe do framework (`onDatalinkMessageEvent`, `shutdownNotification`, ...) | quando o alcance/lado/canal que o subsistema já filtra importa (rádio com `radioName`/`maxRange`, por exemplo) | `xnative::AlertDatalink::onDatalinkMessageEvent()` (`models/flight`) — reage a `DATALINK_MESSAGE`, só alcança quem tem `Datalink` |
| **(b) Broadcast direto** — token próprio (`USER_EVENTS + N`) entregue com `player->event(TOKEN, obj)` varrendo `getWorldModel()->getPlayers()` | quando o efeito é geral/físico e não deve depender de o receptor ter um subsistema específico (explosão, colisão, qualquer "efeito de área") | `events::EID_ALERT`, entregue por `AlertDatalink::broadcastAlert()`, tratado por `GuidedMissile::onAlertEvent()` (`models/missile`) — um player **sem Datalink** reagindo ao mesmo evento |

## Por que o payload mora aqui, numa `shared_library()`, e não no plugin que o define primeiro

Um `dynamic_cast` de um `base::Object*` recebido de **outro** `.so` (carregado por `dlopen`) só é
seguro se a classe do payload vier de uma biblioteca **linkada por todos os lados envolvidos** —
do contrário cada plugin compilado com `gnu_symbol_visibility: hidden` enxerga seu próprio
`type_info` para o "mesmo" tipo, e o `dynamic_cast` falha silenciosamente. É o mesmo motivo, já
documentado no `CLAUDE.md`, de `RLBridgeBehavior` ter ficado **dentro** de `models/flight` em vez
de virar um plugin próprio. A saída aqui é a oposta: em vez de manter o payload preso a um
plugin, ele mora numa `shared_library()` de verdade (esta pasta), publicada pelo SDK
(`dist/lib/`), que **qualquer** modelo pode linkar via `sdk_dep` sem precisar de uma dependência
`meson.build` extra — o mesmo mecanismo já usado por `xboard`/`xlog`/etc. A prominência de pasta
(raiz, não `shared/`) não muda essa obrigação técnica — só reflete que evento é conceito de
primeira classe aqui, não um detalhe de infraestrutura.

Um evento que nunca precisa atravessar fronteira de plugin (só entre classes do mesmo `.so`) não
precisa desse tratamento — pode ficar como uma classe comum do próprio modelo, do jeito que
`xnative::AlertDatalink` sempre fez.

## Registro de tokens alocados

A alocação em si vive em código ([EventTokens.hpp](EventTokens.hpp)), não aqui — esta tabela é só
um índice de leitura rápida, para não colidir números ao adicionar um evento novo:

| token | valor | payload (arquivo) | emitido por | tratado por |
|---|---|---|---|---|
| `events::EID_ALERT` | `USER_EVENTS + 1` | `events::TacticalAlert` ([payloads/EID_ALERT/TacticalAlert.hpp](payloads/EID_ALERT/TacticalAlert.hpp)) | `xnative::AlertDatalink::broadcastAlert()` (`models/flight`) | `xnative::AlertDatalink::onDatalinkMessageEvent()` (via `DATALINK_MESSAGE`, caminho a) **e** `xmissile::GuidedMissile::onAlertEvent()` (via `EID_ALERT` direto, caminho b) |

Próximo token livre: `USER_EVENTS + 2`.

## Caso de referência: o `TacticalAlert` generalizado

`events::TacticalAlert` já existia como `xnative::TacticalAlert`, usado só dentro de
`models/flight` (`AlertDatalink` emite e trata, via o subsistema `Datalink` nativo). Ele foi
promovido para cá — mesma classe, mesmo nome de fábrica `"TacticalAlert"`, nenhuma mudança em
`provides:` de nenhum cenário — e ganhou uma segunda via de entrega (`EID_ALERT`/broadcast
direto) além da original (`DATALINK_MESSAGE`/`Datalink`). Isso prova as duas metades da
convenção ao mesmo tempo: (1) um payload definido uma vez pode ser tratado por mais de um
caminho de despacho, e (2) um handler pode ser escrito num plugin (`models/missile`) sem nenhuma
relação de compilação com quem define ou emite o evento (`models/flight`).

## Um caso futuro conhecido, ainda não implementado

Um evento de "explosão afeta bomba e players num raio" (efeito de área, sem relação com nenhum
subsistema de rádio) é o exemplo natural do caminho (b): um token novo (`EID_EXPLOSION`, por
exemplo) + um payload com epicentro/raio, em `events/payloads/EID_EXPLOSION/`, emitido pelo
modelo da arma ao detonar, tratado por quem quiser reagir (dano, alerta visual, o que fizer
sentido) — sem precisar voltar a discutir esta convenção.
