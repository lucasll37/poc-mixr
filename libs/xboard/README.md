# `libs/xboard` — o quadro de leitura entre host e modelo

Um mapa `playerId → Readout` (rótulo de comportamento, contagem de decisões, thread do pool T/C,
alerta tático, contadores de datalink, varredura de radar) — a **única** coisa que o host
executável e o modelo carregado por `dlopen` compartilham.

## Como se usar

Não há slot de EDL: `xboard` não é uma classe MIXR, é uma API C++ livre, escrita pelo modelo e
lida pelo host. O que o cenário precisa é só carregar um plugin que a use — o `flight`, por
exemplo, via `PluginModule`:

```
components: {
   plugins: ( PluginLoader
      searchPaths: { "./dist/lib/mixr-plugins/" }
      modules: {
         ( PluginModule  file: "libflight_tc.so"
            provides: { AlertDatalink TacticalAlert ThreadTagProbe FlightAgentTC
                        FlightState BtBehavior AltitudeSafetyBehavior
                        RLBridgeBehavior FlightAction } )
      }
   )
   ...
```

**Lado que escreve — dentro do plugin**, no único ponto de atuação comum aos dois agentes
(`SimAgent` de background e `FlightAgentTC` do pool T/C), depois de o `UbfArbiter`/`Fallback` já
ter escolhido o vencedor (`models/players/A-4/src/ubf/FlightAction.cpp`):

```cpp
#include "xboard/Board.hpp"

// antes de sobrescrever -- e o que permite logar a TRANSICAO, nao o estado corrente
const xboard::Readout before{xboard::get(player->getID())};

xboard::setBehaviorLabel(player->getID(), label);   // "EVADE", "SUPPORT", "RTB"...
xboard::bumpDecisionCount(player->getID());         // conta DECISAO, nao candidatura
xboard::setThreadTag(player->getID(), xboard::threadTag());
```

**Lado que lê — no host**, para compor `bt=`/`dec=` no dump determinístico
(`app/src/app/DeterministicDump.cpp`) ou o quadro ao vivo da TUI
(`app/src/app/DashboardState.cpp`):

```cpp
#include "xboard/Board.hpp"

const mixr::xboard::Readout board{mixr::xboard::get(air->getID())};
out << " bt=" << board.label << " dec=" << board.decisions;
```

Um playerId sem nenhuma escrita ainda devolve `Readout{}` (`label="--"`, `decisions=0`,
`threadTag=-1`) — é o valor com que toda aeronave nasce no dump, não um erro.

## Por que esta é a ÚNICA `shared_library()` de `libs/` até este ponto

As demais libs "leves" de `libs/` (`xtacview`, `xclock`, `xjoystick`, `xmsg`) são
`static_library()`. `xboard` não pode ser, e o motivo é estrutural, não de gosto: quem **escreve**
aqui é o modelo, que mora num `.so` carregado com `dlopen`; quem **lê** é o host, que é o
executável. Com uma lib estática cada lado ganharia sua **própria cópia** dos mapas — o modelo
escreveria num, o host leria do outro, e o dump sairia com `bt=--`/`dec=0` para sempre. **Sem erro
de link, sem crash, sem aviso** — só o número errado, silenciosamente, pra sempre. É a mesma
armadilha documentada no cabeçalho de `libs/xplugin/PluginAbi.hpp`, e a saída que
`libs/xplugin/README.md` já registra como a honesta quando um plugin precisa de código
compartilhado: promover a peça a `shared_library()` com SONAME em vez de relaxar a regra de que um
plugin só depende de `mixr_dep` + `xplugin_abi_dep`.

`meson.build` instala a lib (`install_dir: get_option('libdir')`, `install_tag: 'sdk'`) pelo mesmo
motivo: o executável em `dist/bin/` precisa achá-la em `dist/lib/`, e é isso que o rpath
`$ORIGIN/../lib` dos alvos que a consomem espera.

## Concorrência

Escrita acontece nas threads de tempo crítico — a atuação do UBF roda lá, uma vez por aeronave por
frame, e com `FlightAgentTC` isso é **N threads do pool escrevendo ao mesmo tempo**. Leitura
acontece no laço de background do host (o dump/TUI amostra a ~10 Hz). Os seis setters e o `get()`
passam todos pelo MESMO `std::mutex` (o quadro `playerId → Readout`); o mapa em si é minúsculo (4
entradas na produção — uma por falcon), então a contenção não é o problema que a serialização
resolve — é a corrupção de `std::string`/campos compostos sob escrita concorrente.

O registro de `threadTag()` usa um **segundo mutex, separado de propósito** — não o mesmo do
quadro. `threadTag()`/`currentCpu()` identificam **qual thread física do pool** está chamando — um
índice pequeno e estável (0, 1, 2...), com cache `thread_local` que evita tocar esse mutex a cada
chamada no caminho quente (todo player, todo frame; só a primeira chamada de cada thread paga o
lock). Foram promovidas para cá de um contador que
era privado de `models/players/A-4`: com dois plugins distintos no mesmo processo (o modelo de voo e
o míssil guiado, por exemplo), cada `.so` veria sua própria primeira chamada como índice 0, e o
número mostrado na aba Players deixaria de significar "esta é a MESMA thread" entre um avião e um
míssil processados lado a lado no mesmo frame. Com uma única `libxboard.so` compartilhada por
`dlopen`, o contador é o mesmo para qualquer chamador, em qualquer plugin.

## Testes

`tests/domain/test_xboard_concurrency.cpp` (suíte `domain`) — nunca tinha teste direto antes
dele: só validação indireta pelos números `dec=`/`bt=`/`thread=` nos dumps end-to-end. Cobre
incrementos concorrentes exatos (8 threads × 200000, sem perda), escritores concorrentes em
chaves diferentes não se atropelando, os seis setters gravando os campos certos sem vazar um no
outro, estresse combinado de todos os setters no mesmo player (a estrutura nunca corrompe, mesmo
sem atomicidade cruzada entre campos), e a estabilidade de `threadTag()` por thread.
