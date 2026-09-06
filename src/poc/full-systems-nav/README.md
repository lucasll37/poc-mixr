# full-systems-nav — o player máximo, navegando de verdade

Responde a **duas** perguntas ao mesmo tempo:

1. *(built-in_mixr_1 já respondeu a metade)* Qual o player mais elaborado que dá
   para montar usando o máximo de componentes **built-in** do framework?
2. *(esta poc responde a outra metade)* Dá para fazer esse player **voar de
   verdade** pela navegação nativa — `Route`/`Steerpoint`, com as `Action` de
   cada ponto disparando — usando uma árvore de comportamento com **um nó só**,
   plenamente equipada com a roupagem do UBF (`Agent`/`State`/`Behavior`/
   `Action`)?

```bash
make run-full-systems-nav      # tempo real, Tacview na porta 1240
make check-full-systems-nav    # determinismo com 1, 2 e 4 threads T/C

# quem executa é o ./app, o runner único -- esta poc não tem binário próprio
./build/app/src/app -scenario full-systems-nav -deterministic 30000
```

**Esta poc não tem código de C++ próprio no sentido de "aplicação"** — só o
cenário, os dados de execução e este README; quem a executa é o `./app`. Mas
ela **acrescentou uma peça nova ao modelo** (`models/player/A4`): o nó de árvore
`( Navigate )` e um punhado de campos novos em `domain::WorldView` — ver a
seção "O que foi acrescentado ao modelo" abaixo.

## A árvore: um nó só, sem Fallback

`models/player/A4/configs/flight_tree_nav.xml`:

```xml
<root main_tree_to_execute="MainTree">
  <BehaviorTree ID="MainTree">
    <Navigate/>
  </BehaviorTree>
</root>
```

Ao contrário de `flight_tree.xml` (produção: RTB/EVADE/SUPPORT/PATROL), esta
árvore **não sabe fazer mais nada além de seguir a navegação**. Sem
combustível baixo, sem evasão, sem apoio — de propósito: "um BT que só segue
a navegação" quer dizer que ele não tem NENHUM outro comportamento por baixo.

`( Navigate )` (`bt/nodes/NavigateAction.{hpp,cpp}`) faz uma única coisa: lê o
que a `Navigation`/`Route`/`Steerpoint` **nativas** já calcularam
(`domain::WorldView::hasNavSteering`/`navTrueBrgDeg`/`navCmdAltM`/
`navCmdSpeedKts` — populados em `FlightState::updateState()` a partir de
`Player::getNavigation()`) e entrega isso como uma decisão do UBF, pelo MESMO
caminho de atuação que `Patrol`/`ReturnToBase`/`ReportAndEvade` já usam
(`xnative::FlightAction::execute()` → `Autopilot::setCommandedHeadingD/
setCommandedAltitudeFt/setCommandedVelocityKts`). Nenhuma regra de navegação
nova foi escrita — `Route::autoSequencer()`/`Steerpoint::compute()` (nativos,
rodando em `updateData()` incondicionalmente, independente de qualquer
árvore) já fazem todo o trabalho de "para onde ir" e "quando disparar a
Action do ponto que passou".

**Por que `navMode` continua `false` no `pilot:`, mesmo esta poc sendo sobre
navegação**: quem atua é a árvore (via `FlightAction`), não o
`Autopilot::processModeNavigation()` nativo — a mesma convenção de toda
aeronave pilotada por este UBF. Ligar `navMode:true` faria o Autopilot ler a
MESMA `Navigation` por um segundo caminho e disputar o comando com a árvore a
cada frame.

## O agente: `( FlightAgentTC )`, componente do PLAYER

Ao contrário de `built-in_mixr_1` (que usa `( SimAgent )` nativo, componente
da `Station`, decidindo em `updateData()`), esta poc usa `( FlightAgentTC )` —
componente do **player**, decidindo na **fase 3** do frame de tempo crítico,
o mesmo agente que `multi-thread`/`python-flight`/`onnx-policy` e os três
cenários do `./app` já usam. `state:`/`behavior:` são os mesmos três papéis do
UBF de sempre (percepção/decisão/atuação) — só a árvore dentro do `behavior:`
muda. Plugin: `libflight_tc.so` (não `libflight.so`).

## O que foi acrescentado ao modelo (`models/player/A4`)

Nada disto muda `provides:` de nenhum cenário existente (nó de árvore não
tem nome de fábrica — registra na `BT::BehaviorTreeFactory`, não na factory
MIXR):

- `domain::WorldView` ganhou seis campos de leitura da navegação nativa:
  `hasNavSteering`/`navTrueBrgDeg` (de `Navigation::isNavSteeringValid()`/
  `getTrueBrgDeg()`) e `hasNavCmdAlt`/`navCmdAltM`/`hasNavCmdSpeed`/
  `navCmdSpeedKts` (do `Steerpoint` "to" corrente, via `Route::getSteerpoint()`).
- `FlightState::updateState()` popula esses seis campos a partir de
  `air->getNavigation()` — nenhum tipo do MIXR vaza para fora dele; a árvore
  continua compilando sem MIXR (`bt_sources`, camada `tree`).
- `bt/nodes/NavigateAction.{hpp,cpp}` (novo nó, `"Navigate"`) e o registro em
  `bt/bt_factory.cpp`.
- `configs/flight_tree_nav.xml` (novo, instalado junto com os outros
  `flight_tree_*.xml` em `dist/share/mixr-plugins/flight/`).

## Armadilha nova, medida rodando — não redescobrir

**`Navigation::getTrueBrgDeg()` é marcação direto-ao-ponto, recalculada A
CADA FRAME a partir da posição atual (`Steerpoint::compute()`,
`Route.cpp`/`Steerpoint.cpp`) — é o MESMO dado que
`Autopilot::processModeNavigation()` consultaria com `navMode` ligado
(`Navigation::updateNavSteering()` faz exatamente
`setTrueBrgDeg(to->getTrueBrgDeg())`).** Comandar isso direto como rumo, a
cada tick, sem nenhum amortecimento, é perseguição pura (*pure pursuit*) sem
termo de avanço — a mesma classe de problema já documentada na seção "Demo:
míssil guiado" deste repositório para `domain::pursuit()` (*"um controlador
guiagem só proporcional diverge"*). `( Navigate )` tem um limitador de taxa no
rumo comandado (`kMaxHeadingRateDegPerSec`, `NavigateAction.cpp`) pela MESMA
razão e a MESMA técnica.

**Mas o limitador de rumo não foi a causa do acidente medido, e isolar isso
foi o trabalho real desta poc.** Rodando sem ele, a aeronave espiralava e
caía no terreno em ~200 s simulados — só que **desligando completamente a
navegação e travando o rumo comandado em 90° fixo o tempo todo**, a MESMA
queda continuava, idêntica, byte a byte nos primeiros milhares de frames.
Isso eliminou rumo/guiagem como causa. A causa de verdade: os `airspeed:` de
cada `Steerpoint`, copiados sem pensar do `built-in_mixr_1` (160-170 kt) —
LÁ esse campo é só metadado decorativo (a árvore de produção pilota via
`patrolSpeed: 350.0`, e o valor do steerpoint nunca chega a ser lido para
voar). Aqui, `( Navigate )` **lê de verdade**
`Steerpoint::getCmdAirspeedKts()` e manda para o `Autopilot` via
`velocityHoldMode`. 160 kt está bem abaixo da faixa que este `JSBSimModel`
"A4" sustenta em voo nivelado (a mesma faixa de `patrolSpeed`/`rtbSpeed`/
`evadeSpeed`/`supportSpeed` da árvore de produção: 350-420 kt) — comandado a
manter altitude E 160 kt ao mesmo tempo, o avião não consegue as duas coisas,
e as duas caem juntas, continuamente, até o CFIT. Os quatro `airspeed:` desta
poc (e o `initVelocity:` do player) foram corrigidos para a faixa de 350-370
kt; **medido rodando 30.000 frames (600 s simulados) sem cair uma vez**,
altitude e velocidade convergindo e estabilizando em cada perna da rota.

## O que foi medido rodando (30.000 frames, `-deterministic`)

- Zero erro de parse, zero `"was not found!"` — os mesmos 53 componentes
  nativos de `built-in_mixr_1`, um único player.
- `bt=NAV` em 100% das linhas do dump; `dec=` avança na mesma taxa que
  `frame` (offset de 1 na partida, o mesmo *warm-up tick* documentado para
  toda poc baseada em `FlightAgentTC`).
- A aeronave completa a volta inteira repetidamente (`wrap: true`): rumo
  varre 360° a cada laço, altitude/velocidade convergem para o valor
  comandado por cada `Steerpoint` (1750/1900/1750/1600 m; 350/370/350/360 kt)
  e se estabilizam — nunca mais voltam a cair sem controle.
- `ActionWeaponRelease` do steerpoint 4 dispara de verdade: o flyout
  `W10001` aparece no `MsgFeed` (`mission.jsonl`) e no Tacview
  (`mission.acmi`), confirmando que a `Route`/`Steerpoint` nativa está
  disparando ações reais, não só recalculando números.
- Determinismo preservado: dumps `frame=` byte-idênticos com 1, 2 e 4
  threads de tempo crítico (`make check-full-systems-nav`).

## Por que o cenário não se chama `scenario.edl.in`

`tests/guard/check_falcons_estrutura.sh` varre esse nome por glob e exige
`falcon1..4` com o mesmo esqueleto de slots — este cenário tem **um** player
só, chamado `a4`. Mesmo recurso que `built-in_mixr_1`/`bandit` já usam.
Consequência igual à deles: esta poc não entra na lista `pocs` de
`tests/meson.build` (as suítes `scenario`/`memory` derivam fixtures de
`configs/scenario.edl.in`); o determinismo tem alvo próprio
(`make check-full-systems-nav`), rodando contra o cenário desta pasta — já
hermético, sem `networks:`.

## O que herda de `built-in_mixr_1`, sem mudança

Os 53 componentes nativos de `falcon1` (dez sistemas primários +
`CollisionDetect` + assinaturas `SigSwitch`/`IrSignature`) foram copiados
quase byte a byte — ver `built-in_mixr_1/README.md` para a tabela completa e
as oito armadilhas de montagem (todas continuam valendo aqui: `Table2` é
lista de listas, `sarLatitude`/`targetLatitude` são `LatLon` não `Angle`,
`ActionWeaponRelease` ignora `station:`, `dataLogTime:` obrigatório em cada
store liberável, etc.). As diferenças são: um player só (não quatro +
intruso), altitude/velocidade variadas por steerpoint (1750→1900→1750→1600 m,
350→370→350→360 kt, em vez do valor único de `patrolSpeed`), e
`numToLaunch: 1` no decoy (não 2 — sem árvore de patrulha "escapando" do
circuito, as Action disparam TODA VOLTA, então uma rajada de 2 por passagem
viraria uma rajada indefinida ao longo do cenário).
