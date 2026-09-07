# `libs/xclock` — controle de velocidade do tempo simulado

`( ClockStation )` no lugar de `( Station )` na raiz do cenário dá à aplicação acelerar, frear em
câmera lenta e pausar de verdade — nenhuma dessas três coisas o MIXR nativo oferece completas.

## Como se usar

### No `.edl`: trocar a raiz, e só

`ClockStation` **é** uma `simulation::Station` (`DECLARE_SUBCLASS(ClockStation,
simulation::Station)`), sem slot próprio (`EMPTY_SLOTTABLE`) — troca-se o nome da classe na raiz
do cenário e nada mais muda: `RESET_EVENT`, `WorldModel`, `players`, `dataRecorder`, `ioHandler`
continuam exatamente como num `( Station )` comum. Do início real de
`src/poc/dis/single-thread/configs/scenario.edl.in`:

```
( ClockStation

   tcPriority: 0.5

   ownship: falcon1

   components: {
      plugins: ( PluginLoader ... )
      ...
   }
)
```

A velocidade inicial continua sendo o slot **nativo** `fastForwardRate` da própria `Station` — a
lib não introduz um slot de escala; ela só passa a reagir a ele em runtime.

Para o cenário carregar, a factory da lib precisa estar encadeada **antes** de
`mixr::simulation::factory()` (que responde por `"Station"`) — é o próprio `ClockStation` que tem
nome de fábrica próprio e precisaria perder a corrida contra `"Station"` senão. Em
`app/src/mixr_factory.cpp`:

```cpp
if (obj == nullptr) obj = mixr::xclock::factory(name);
// ... antes de mixr::simulation::factory(name) mais adiante na cadeia
```

Trocar de volta para `( Station )` continua rodando — só sem as teclas/API de tempo (o `main.cpp`
avisa em `stderr` e segue, ver `app/src/app/StationBuilder.cpp::clockStationOf()`).

### Em C++: a API de tempo

`ClockStation` expõe métodos públicos comuns — nenhuma cerimônia de slot para quem já tem o
ponteiro. É o que `app/src/app/DashboardLoop.cpp` chama direto, na thread de UI, para os botões/
teclas de acelerar, frear e pausar:

```cpp
#include "xclock/ClockStation.hpp"

// acelerar / frear: percorre uma escada de degraus (0.10x .. 64x, ver TimeControls
// mais abaixo) e escreve o degrau escolhido.
clockStation->setTimeScale(4.0);     // >= 1x: vira o fastForwardRate nativo (arredondado)
clockStation->setTimeScale(0.25);    // < 1x: liga a camera lenta (ver "o que é nativo" abaixo)

clockStation->togglePaused();        // pausa / retoma
clockStation->setPaused(false);      // retoma explicitamente (ex.: "voltar a tempo real")

clockStation->getTimeScale();        // 1.0 = tempo real
clockStation->isPaused();
```

Para encerrar o processo sem deixar a thread de tempo crítico correndo contra o teardown (ver
"parada segura" abaixo), o padrão real é o de `app/src/app/Shutdown.cpp`:

```cpp
clockStation->setTimeScale(1.0);          // volta a 1x antes de pedir a parada
clockStation->requestTcStop();
if (!clockStation->waitForTcQuiesced(timeoutSec)) {
   LOG(WARNING) << "thread de tempo critico nao confirmou ociosidade";
}
```

### A alternativa por teclado: `TimeControls` + `ConsoleKeyboard`

A lib também traz um par pronto para ligar teclas de console ao relógio — pensado para um laço de
tempo real "de texto simples" (o que as pocs tinham antes de existir uma TUI própria), e
compartilhado entre subprojetos para que todos usem as **mesmas** teclas:

```cpp
#include "xclock/TimeControls.hpp"

mixr::xclock::TimeControls controls(clockStation);
// a cada iteracao do laco de background:
if (controls.poll()) { /* algo mudou: reimprimir a linha de status */ }
std::cout << "[" << controls.describe() << "]";   // "4x" ou "PAUSADO (4x)"
```

Teclas: `+`/`=` acelera, `-`/`_` freia, `espaço`/`p` pausa, `1` volta a 1x e retoma, `h`/`?`
reimprime a ajuda. `ConsoleKeyboard` é `termios` em modo bruto, não bloqueante, e **não** é um
`mixr::linkage::IoDevice` de propósito — não há canal nomeado nenhum para casar, só métodos
públicos comuns da `ClockStation` (ver o cabeçalho de `ConsoleKeyboard.hpp`). Sem TTY (pipe,
`-deterministic`, CI) `tcgetattr()` falha, `isAvailable()` vira `false` e a simulação roda
normalmente, só sem teclado.

`./app`, hoje o runner único das pocs, **não** usa este par — FTXUI já é dono do terminal
(`ScreenInteractive::Fullscreen()`) desde antes do laço começar, e disputar `termios` por fora
dele quebraria o desenho; `DashboardLoop.cpp` chama `setTimeScale()`/`togglePaused()`/
`setPaused()` direto (ver o comentário correspondente no próprio arquivo). `TimeControls`/
`ConsoleKeyboard` seguem compilados e testáveis, prontos para qualquer laço que não tenha uma UI
disputando o terminal.

## Divisão deliberada entre nativo e próprio

- **Acelerar (≥ 1x) é 100% nativo.** `Station::processTimeCriticalTasks()` já faz
  `for (jj=0; jj<getFastForwardRate(); jj++) tcFrame(dt);` — `setFastForwardRate()` é público e
  virtual, muda em runtime sem nada além de chamá-lo.
- **Frear (< 1x) não existe no framework** — `fastForwardRate` é `unsigned int`, só multiplica, e
  não há setter público para baixar a taxa da própria thread T/C em runtime. A **única** coisa que
  esta classe acrescenta é um override de `processTimeCriticalTasks()` que, abaixo de 1x, roda um
  único `tcFrame(dt * slowFactor)` — passo de integração **menor**, nunca maior, então a dinâmica
  (JSBSim inclusive) não degrada.
- **Pausar é nativo, por um caminho não óbvio.** Não existe `Simulation::pause()`; existe o flag
  de freeze do `base::Component`, testado por `Simulation::updateTC()`/`updateData()`
  (`if (isFrozen()) dt0 = 0.0`). A cascata não é *push* para os filhos — é *consulta*, no sentido
  inverso (`Player::isFrozen()` testa o próprio flag OU o da simulação, `System::isFrozen()` o
  próprio OU o do ownship, `Player::dynamics()` repassa ao `DynamicsModel`). Por isso
  `setPaused()`/`isPaused()` agem em `getSimulation()`, **não** na própria `Station` — congelá-la
  não pararia nada disso.

## A armadilha do `execTime`, e por que pausar tem de pular `tcFrame()`

Só marcar o freeze não bastaria: `Simulation::updateTC()` faz `execTime += dt` **antes** do teste
de `isFrozen()`, com o `dt` cru. Ou seja, com a simulação congelada o mundo para mas
`getExecTimeSec()` continua correndo — e isso vazaria para o Tacview, que data cada linha ACMI com
`exec_time`: o replay avançaria com as aeronaves paradas (medido rodando, antes da correção: com a
simulação congelada, `sim=` ainda subia). `ClockStation::processTimeCriticalTasks()` resolve os
dois de uma vez simplesmente **não chamando `tcFrame()`** quando pausado — o relógio de execução
para junto com o mundo, e de quebra evita integrar um estado que não muda. O flag de freeze
continua marcado porque é ele que congela o outro caminho, o de background
(`Simulation::updateData()`, que tem o mesmo teste e não passa por aqui).

## Parada segura da thread T/C — `requestTcStop()`/`waitForTcQuiesced()`

O MIXR não oferece API para parar a `StationTcPeriodicThread` de forma limpa: o laço dela só testa
`getParent()->isShutdown()`, flag setada **depois** de a `Simulation` já ter sido derrubada — na
janela entre as duas coisas a thread ainda começa frames novos e pode se auto-travar chamando
`SyncThread::waitForAllCompleted()` sobre workers que já morreram (o comentário completo do
"porquê", com os arquivos/linhas do framework, está em `ClockStation.hpp`). Além do risco de
deadlock, enquanto essa thread roda ela segue enfileirando registros numa fila do
`recorder::OutputHandler` **sem teto** — se o laço de background da aplicação já parou de drenar,
isso é memória crescendo sem limite.

A saída é não deixar frame novo **começar**: `requestTcStop()` marca uma flag (idempotente, segura
de qualquer thread) e `waitForTcQuiesced(timeoutSec)` espera a **prova positiva** de que a thread
T/C passou por `processTimeCriticalTasks()` depois da marcação — logo, que o `tcFrame()` anterior
já retornou —, com teto de tempo (nunca trava o chamador). Sem thread T/C nativa (`-deterministic`,
que chama `tcFrame()` direto na própria thread do laço) não há nada para esperar e o método
devolve `true` na hora. Ver o padrão real de uso em `app/src/app/Shutdown.cpp::quiesceTimeCritical()`.

## Limite conhecido, documentado no próprio header

`ubf::Agent::updateData()` (`models::SimAgent`, usado por `src/poc/dis/single-thread`) chama
`controller(dt)` sem consultar `isFrozen()` — com a simulação pausada esses agentes continuam
decidindo, só que sobre um mundo estático: nada se move, a decisão apenas não para. O
`FlightAgentTC` de `src/poc/dis/multi-thread`, decidindo na fase 3 do frame, para junto — porque
`tcFrame()` simplesmente não roda enquanto pausado.

## Por que é `static_library()`, não `shared_library()`

`xclock` só fala com a `Station`/`Simulation` nativas — nunca cruza a fronteira `dlopen` de plugin
(nenhum modelo em `models/players/` inclui este header). Por isso fica estática, como
`xtacview`/`xjoystick`/`xmsg`/`xplugin`: as seis libs que viram `shared_library()`
(`xboard`/`xlog`/`xtrack`/`xrlbridge`/`xinfer`/`xpyembed`) só existem porque host e plugin
precisam compartilhar uma cópia só de estado mutável através do `dlopen`. `xclock` não tem esse
requisito — promovê-la a `shared_library()` daria a um plugin uma cópia própria do estado, sem
benefício nenhum (ver `.claude/rules/host-app-src.md`).

## Testes

Não há suíte dedicada a `xclock` isolada (sem `Station`/thread real não há o que exercitar do
freeze/T/C). Quem prova o caminho de parada em produção é `scenario-app-quit`/
`scenario-app-quit-dis` (`tests/meson.build`, suíte `scenario`, `tests/scenario/
run_app_quit_test.py`) — sobem `./app` sob pty, mandam sair pela UI e conferem que o processo
encerra dentro do prazo, inclusive com um cliente Tacview pendurado que forçaria a fila do
gravador a crescer sem a parada correta da thread T/C.
