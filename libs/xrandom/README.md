# `libs/xrandom` — derivação de sementes reprodutíveis

**Duas camadas, e este é o único lugar do repositório onde qualquer uma das duas existe.**

1. **Derivação de sementes** — `fnv1a64(nome)` hasheia uma identidade estável e
   `deriveSeed(seed, salt)` mistura duas sementes numa terceira. Puras, `constexpr`, sem estado.
2. **O gerador** — a classe `Rng`, que embrulha o `std::mt19937_64` e as distribuições.
   **Nenhuma outra classe do repositório instancia um gerador**: os consumidores guardam um `Rng`.

Um `grep` por `mt19937`/`uniform_real_distribution` fora desta pasta tem de voltar vazio — é
essa a propriedade que a lib existe para manter.

## Como se usar

Único consumidor real hoje: `BtBehavior::configurePlans()`
(`models/players/A-4/src/ubf/BtBehavior.cpp`), que já depende do SDK. O `.edl` de produção declara
a **mesma** `patrolMasterSeed` nos quatro falcons — a variação vem do nome de cada um, não do
literal:

```
behavior: ( BtBehavior
   ...
   // Jitter de rumo na patrulha (opcional): mesma patrolMasterSeed nos 4
   // falcons -- cada um deriva a propria sequencia do proprio NOME, nunca
   // de ordem de descoberta/thread.
   patrolJitterHeading: ( Degrees 6 )
   patrolMasterSeed:    20260903
   ...
)
```

E o C++ que resolve isso a cada player, em duas derivações — master→instância (salt = hash do
nome) e instância→propósito (salt = uma constante fixa por consumidor):

```cpp
#include "xrandom/DeterministicRng.hpp"

constexpr std::uint64_t kPatrolJitterSalt{0x5041'5452'4F4C'4A00ULL}; // "patrol jitter"

const std::uint64_t instanceSeed = tune.patrolSeedOverrideSet
   ? tune.patrolSeedOverride
   : xrandom::deriveSeed(tune.patrolMasterSeed, xrandom::fnv1a64(playerName));

patrol.setHeadingJitter(tune.patrolJitterHeadingDeg,
                         xrandom::deriveSeed(instanceSeed, kPatrolJitterSalt));
```

`patrolSeedOverride` (opcional, num player só) pula a derivação por NOME sem pular a de PROPÓSITO
— escape hatch para um teste que quer uma semente literal, não usado em produção.

## O gerador (`Rng`)

```cpp
mixr::xrandom::Rng rng{sementeJaDerivada};

rng.uniform(minSec, maxSec);   // [lo, hi); faixa degenerada devolve 'lo' SEM consumir
rng.symmetric(amplitude);      // [-a, +a); a <= 0 devolve 0 SEM consumir
rng.seed(outra);               // reset de verdade: volta ao início da sequência
rng.seedValue();               // a semente em vigor
```

Três decisões que estavam **repetidas em cada consumidor** e agora são tomadas uma vez:

- **a distribuição é construída a cada chamada, nunca guardada.** Uma
  `std::uniform_real_distribution` guardada tem estado próprio em algumas implementações, e esse
  estado sobreviveria a um `seed()` — resemear não voltaria ao início. Travado por
  `Rng.ResemearVoltaAoInicioDaSequencia`.
- **faixa/amplitude degenerada não consome o gerador.** É como um consumidor desliga a variação
  (`patrolJitterHeading: 0`) ou fixa um intervalo (`slowRollMaxInterval <= slowRollMinInterval`)
  sem deslocar a sequência de quem ainda sorteia. Travado por dois testes.
- **não há gerador global, nem construtor que "invente" uma semente** (nada de
  `std::random_device`). Um `Rng` recém-construído tem semente `0` — válida e reprodutível como
  qualquer outra. Aleatoriedade que muda a cada execução não tem lugar neste projeto.

Consumidores hoje: `domain::PatrolPlan` (jitter de rumo) e `domain::AerobaticPlan` (intervalo
entre acrobacias), os dois em `models/players/A-4`, os dois semeados por
`BtBehavior::configurePlans()` a partir do **mesmo** `instanceSeed` com salts de propósito
diferentes.

## Como `domain/` pode incluir um header do SDK

`domain/` não linka o SDK — `test_domain`/`test_tree` compilam `domain_sources` **sem MIXR**, e
essa propriedade é o que permite testar a árvore de produção em ~10 ms sem levantar `Station`.
Enquanto o gerador estava aqui e os consumidores em `domain/`, a saída foi duplicar o
`std::mt19937_64` dentro de cada um.

O que destravou a unificação: este header é **header-only e sem dependência nenhuma**
(`<cstdint>`, `<random>`, `<string_view>`), então incluí-lo não arrasta MIXR. Basta dar aos dois
alvos de teste o **caminho de include** do SDK, nunca o link:

```meson
sdk_headers_dep = sdk_dep.partial_dependency(includes: true, compile_args: true)
```

`sdk_dep` inteiro traria `Requires: mixr` do `.pc` para o link e destruiria a propriedade em
silêncio. Conferido com `ldd` nos dois binários: zero libs do MIXR.

## Por que a sub-semente vem do NOME, nunca de ordem

A ordem de descoberta/processamento entre players **não é garantida** neste framework — todo
agente decide em paralelo, um player por thread do pool de tempo crítico. Um esquema que
distribuísse sementes por posição numa lista (`0, 1, 2, 3`) divergiria entre 1/2/4 threads T/C,
porque essa ordem muda. Um hash da identidade do próprio player (`fnv1a64(getName())`) elimina
qualquer coordenação: cada `BtBehavior` calcula a própria semente sozinho, sem saber nada sobre
os outros nem em que thread está. É a propriedade que
`tests/determinism/check_patrol_seed.sh` trava ponta a ponta; `deriveSeed` sozinho, sem
`Station`, é o que `tests/domain/test_xrandom.cpp` testa na unidade.

`deriveSeed` usa splitmix64, não soma/xor — duas derivações do MESMO `instanceSeed` com salts
diferentes (ex.: `PatrolPlan` e um segundo consumidor futuro) precisam de sequências **sem
correlação** entre si, não só sementes numericamente distintas.

## Por que é header-only, ao contrário de xboard/xlog/xtrack/xrlbridge/xinfer/xpyembed

As outras seis `libs/x*` viram `shared_library()` porque host e plugin precisam compartilhar UMA
cópia de estado mutável em tempo de execução através do `dlopen` (ex.: `xlog::setLoggingEnabled()`
tem que alcançar o `.so` do modelo). Derivação de semente não tem esse requisito — `seed` entra,
número sai, sem estado global nenhum — então vira só mais um `install_headers()` no `meson.build`
raiz, no mesmo molde de `libs/xplugin/PluginAbi.hpp`.

A classe `Rng` **não** muda isso: ela não tem estado global, só o do próprio objeto, e cada
consumidor guarda o seu. O que ela precisou foi do caminho de include nos alvos de teste — ver a
seção acima.
