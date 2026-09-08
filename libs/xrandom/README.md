# `libs/xrandom` — derivação de sementes reprodutíveis

Duas funções puras, `constexpr`, sem estado: `fnv1a64(nome)` hasheia uma identidade estável,
`deriveSeed(seed, salt)` mistura duas sementes numa terceira. Não é um gerador — é a camada que
decide *qual* semente cada consumidor usa, deixando o `std::mt19937_64` de verdade dentro de quem
consome (ver o "porquê" no cabeçalho de [`DeterministicRng.hpp`](DeterministicRng.hpp)).

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

É também por isso que `domain::PatrolPlan` **não** inclui este header: `models/players/A-4/tests/`
compila `domain_sources` sem o SDK (sem MIXR), e este header só fica visível via `dist/include`
(publicado pelo SDK). `PatrolPlan` mantém seu próprio `std::mt19937_64` privado, seedado por um
`std::uint64_t` que já chega pronto — a classe não sabe de master seed, nome de player nem salt de
propósito.
