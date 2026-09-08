# `libs/xjoystick` — controle do ownship por joystick físico

Aplica roll/pitch/leme/manete de um `UsbJoystick` nativo direto no `AirVehicle` indicado por
nome, desengatando o `Autopilot` scripted enquanto o hardware estiver presente.

## Como se usar

Tudo mecanismo **nativo** do MIXR (`mixr::linkage` — `IoData`/`AnalogInput`/`UsbJoystick`, já
parte da `mixr_dep`) mais um `( JoystickIoHandler )` desta lib, no slot `ioHandler:` da
`Station`. Bloco real, de `src/poc/dis/bandit/configs/scenario.edl`:

```
   ioHandler: ( JoystickIoHandler
      player: "bandit1"
      deviceIndex: 0
      inputData: ( IoData numAI: 4 )
      devices: {
         ( UsbJoystick
            deviceIndex: 0
            adapters: {
               ( AnalogInput ai: 1  channel: 0 )                          // ROLL_AI     <- aileron
               ( AnalogInput ai: 2  channel: 1 )                          // PITCH_AI    <- profundor
               ( AnalogInput ai: 3  channel: 2 )                          // RUDDER_AI   <- leme
               ( AnalogInput ai: 4  channel: 3  offset: 1.0  gain: -1.0 ) // THROTTLE_AI <- manete
            }
         )
      }
   )
```

- **`player:`** — nome do player a controlar (resolvido em runtime via `Simulation::getPlayers()`,
  não em tempo de parse).
- **`deviceIndex:`** no `JoystickIoHandler` tem que **bater** com o `deviceIndex:` do
  `( UsbJoystick )` dentro de `devices:` — é a mesma checagem de existência de arquivo
  (`hasRealJoystick()`) e a mesma leitura (`getAnalogInput()`) que precisam apontar pro mesmo
  dispositivo físico.
- Os quatro `ai:` (1 a 4) são os canais lógicos do `IoData`, definidos como `ROLL_AI`/`PITCH_AI`/
  `RUDDER_AI`/`THROTTLE_AI` em [`ChannelMap.hpp`](ChannelMap.hpp) — únicos entre este `.cpp` e o
  EDL para não repetir o número em dois lugares. Os `channel:` (0 a 3) são os canais **físicos**
  do dispositivo, confirmados para um Logitech Extreme 3D com `tools/joystick_mapper.py`.

Nenhuma classe/factory própria precisa ser encadeada por quem escreve o cenário — o host já
encadeia `mixr::linkage::factory` e a [`factory()`](factory.hpp) desta lib antes dela.

Do lado C++, quem quiser consultar o handler (não é o caso comum — o `.edl` acima já basta)
usa a API pública herdada de `linkage::IoHandler`: `inputDevices(dt)`/`outputDevices(dt)`,
chamadas pelo laço de background do runner (`app`/pocs) na mesma cadência do resto do
`updateData()`.

## Armadilhas confirmadas

1. **`ai:`/`di:` é 1-based; `channel:` é 0-based.** Convenção do próprio `IoData` (ver
   `IoData.hpp`) contra a convenção do dispositivo físico — os dois números aparecem lado a lado
   no bloco acima e não são o mesmo eixo contado de jeitos diferentes por acaso: um é índice
   lógico, o outro é índice de hardware.
2. **Sem joystick conectado, a degradação é muda, e é por isso que este handler existe.**
   `UsbJoystick` nativo (Linux) só loga um aviso quando `/dev/js<N>`/`/dev/input/js<N>` não
   existe — os canais ficam em zero e `getAnalogInput()` devolve `false`, sem exceção. Quem
   chama não tem como distinguir "zero porque não há dispositivo" de "zero porque o piloto
   centralizou o manche". `JoystickIoHandler::hasRealJoystick()` confere a existência do arquivo
   **ele mesmo**, na mesma ordem de busca de `UsbJoystick_linux.cpp`, antes de tocar em qualquer
   coisa: sem o arquivo, nem desengata o `Autopilot` nem aplica stick — o player continua
   exatamente como ficaria sem nenhuma seção `ioHandler:`. A checagem roda todo frame (um
   `stat()`, custo desprezível): plugar o joystick no meio de uma execução já em andamento troca
   para controle manual sem reiniciar nada.
3. **O `Autopilot` tem de ser desligado a CADA frame, não só na primeira leitura.** O player alvo
   normalmente tem `( Autopilot headingHoldMode/altitudeHoldMode/velocityHoldMode: true )`, que
   reimpõe esses três modos a cada fase do frame de tempo crítico (até 50 Hz) — mais rápido que a
   sondagem deste handler no laço de background (10 Hz). `inputDevicesImpl()` localiza o
   `Autopilot` do player (`Player::getPilotByType`) e desliga os três hold modes **todo frame**,
   antes de aplicar o stick direto no `AirVehicle` — não pelo `Autopilot`. Isso só acontece
   quando há joystick de verdade (armadilha 2): sem hardware, o `Autopilot` fica intocado e segue
   no controle.
4. **O manete precisa de `offset:`/`gain:` juntos, para inverter E reescalar no mesmo passo.** O
   eixo físico do slider do Extreme 3D sai em `-1.0` no batente de potência plena e `+1.0` no de
   cutoff. `DynamicsModel::setThrottles()` é unidirecional — `0.0` idle, `1.0` MIL, `2.0`
   pós-combustão (ver o comentário de `DynamicsModel.hpp`; é a mesma faixa `[0.0, 2.0]` que o
   `.cpp` clampa antes de chamar `setThrottles()`) — ao contrário de roll/pitch/leme (`-1..1`,
   sem transformação nenhuma). É por isso que só o `ai: 4` leva `offset: 1.0 gain: -1.0`: o
   `AnalogInput` calcula `t = (raw - offset) * gain`, e essa combinação inverte e reescala de
   `[-1,1]` para `[0,2]` no mesmo passo (`raw=-1 → t=2.0`; `raw=+1 → t=0.0`).
   **CORRIGIDO (não redescobrir):** o `gain` já foi `-0.5` — a conta batia sozinha, mas só
   alcançava `[0,1]` (nunca saía de MIL); achado por auditoria, sem verificação com joystick
   físico depois da troca.
5. **WSL2 não repassa USB por padrão.** O binário é o mesmo nos dois ambientes; o que muda é
   operacional — em WSL2 é preciso `usbipd-win` no host Windows
   (`usbipd attach --wsl --busid <id>`) para o joystick aparecer em `/dev/input/js*` dentro da
   VM. Em Linux nativo basta o módulo de kernel `joydev` carregado.

## Por que é `static_library()`, e não `shared_library()`

Só fala com `mixr::linkage` (já parte da `mixr_dep`) e com `Station`/`Simulation`/`AirVehicle`
nativos — nenhuma dependência nova (nem SDL, nem evdev): o `UsbJoystick` nativo lê
`/dev/input/jsX` com `ioctl` cru. Nada aqui cruza a fronteira `dlopen` de plugin (ver
`.claude/rules/host-app-src.md`), então não há razão para pagar o preço de uma `.so` — um plugin
que a linkasse ganharia cópia própria do estado estático, quebrando o compartilhamento que
justifica `xboard`/`xlog`/`xtrack`/`xrlbridge`/`xinfer`/`xpyembed` serem `shared_library()`.

## Testes

`meson test -C build --suite domain -R xjoystick` (`tests/domain/test_xjoystick.cpp`, alvo
`test-xjoystick`) cobre a camada isolável sem `Station`/`Simulation`/`AirVehicle`/dispositivo
físico: construção/destruição, os dois setters de slot (`player`/`deviceIndex`) e o caminho de
degradação sem hardware (`inputDevices()`/`outputDevices()` não tocam em nada e não derrubam o
processo). As armadilhas de verdade — desengate dos hold modes, inversão de sinal do manete,
numeração `ai:`/`channel:` — só são exercitadas manualmente, rodando o binário de `bandit` com
hardware físico conectado (o próprio arquivo de teste documenta essa lacuna).
