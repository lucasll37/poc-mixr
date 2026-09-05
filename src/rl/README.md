# src/rl -- wrapper Gymnasium sobre a simulacao MIXR/flight

Um agente de RL/Python controla **uma** aeronave (`falcon1`, por padrao) do
mesmo cenario que `single-thread`/`multi-thread`/`app` ja rodam -- mesmo
plugin `libflight_tc.so`, mesma pilha nativa. O `state` do `gymnasium.Env` e
a mesma percepcao que o UBF ja usa para decidir (`domain::WorldView`); a
`action` e o mesmo comando que `xnative::FlightAction` ja aplica no
`Autopilot` (heading/altitude/speed). As outras aeronaves do cenario
(`falcon2..4`) continuam decidindo pela arvore de comportamento nativa --
trafego/companheiros de cenario, nao controlados pelo agente RL (v1 e
single-agent; ver "Escopo" no plano de implementacao original desta
feature).

## Arquitetura

```
Python (gymnasium.Env)                    C++ (processo unico, Station viva)
─────────────────────                     ──────────────────────────────────
MixrFlightEnv.reset()      ──call──>      NativeSimulation::reset()
                            <──obs dict──   station->event(RESET_EVENT) + prime

MixrFlightEnv.step(action) ──call──>      NativeSimulation::step(cmd)
                                             1. xrlbridge::setPendingCommand(cmd)
                                             2. station->tcFrame(dt); station->updateData(dt);
                                             3. RLBridgeBehavior::genAction() (dentro do
                                                tcFrame acima) consome o comando e publica
                                                a observacao deste frame em xrlbridge
                            <──obs,term──   xrlbridge::getObservation() + Player::isCrashed()
```

Um modulo de extensao **pybind11** (`src/rl/bindings/`, compila para
`_native*.so`) mantem a `Station` viva no MESMO processo Python -- sem
round-trip de rede por passo. A troca de comando/observacao entre o host
(este modulo) e o modelo (`models/flight`, um `.so` carregado por `dlopen`)
passa por `shared/xrlbridge` -- uma shared_library pequena, dedicada,
mesmo motivo estrutural de `shared/xboard::Board` (ver o cabecalho de
`shared/xrlbridge/RLBridge.hpp`): o host **nao pode** incluir headers do
modelo nem linkar contra o `.so` dele em tempo de compilacao
(`tests/guard/check_host_opaco.sh` trava esse invariante), entao a troca
so pode passar por uma peca que os dois lados linkam de verdade.

`models/flight/include/ubf/RLBridgeBehavior.hpp` e o `AbstractBehavior` que
faz esse papel do lado do modelo -- entra no `UbfArbiter` de `falcon1` no
lugar de `BtBehavior`, ao lado do MESMO `AltitudeSafetyBehavior` (voto 90,
maior que o voto 50 do bridge) que ja protege as outras aeronaves: uma
politica ruim do agente RL nao derruba o aviao no terreno, o arbitro nativo
sobrepoe.

## Build

```bash
make configure   # inclui pybind11 (conanfile.py) alem das dependencias de sempre
make sdk         # publica libxboard/libxlog/libxtrack/libxrlbridge + headers em dist/
make models      # compila libflight_tc.so (com RLBridgeBehavior) -> plugins/
make build       # compila o host, incluindo o modulo _native (src/rl/bindings/)
make install     # dist/python/mixr_gym/{__init__.py, env.py, _native*.so}
```

Ou o fluxo de sempre: `make configure && make build && make install`.

## Rodando

```bash
pip install -r src/rl/requirements.txt

# cwd tem de ser a RAIZ do repositorio -- mesma convencao de todo binario
# deste projeto (os caminhos de configs:/data: no .edl sao relativos).
PYTHONPATH=./dist/python python3 src/rl/tests/test_smoke.py
```

Uso basico:

```python
from mixr_gym import MixrFlightEnv

env = MixrFlightEnv()   # scenario_path default: ./src/rl/configs/scenario_rl.edl
obs, info = env.reset()

for _ in range(1000):
    action = env.action_space.sample()   # [headingDeg, altitudeM, speedKts]
    obs, reward, terminated, truncated, info = env.step(action)
    if terminated or truncated:
        obs, info = env.reset()

env.close()
```

Tacview (porta 1237, opcional, so para acompanhar visualmente um episodio):
aponte o Tacview Real-Time Telemetry para `<host>:1237` enquanto o processo
Python estiver rodando.

**Isto aqui e so o AMBIENTE.** `src/rl/requirements.txt` fica minimo de proposito
(so o que basta pra importar `mixr_gym` e rodar o smoke test acima) -- nenhuma
dependencia de algoritmo de RL entra aqui. Quem de fato treina uma politica
contra este ambiente e o consumidor em [`src/poc/rl-training/`](../poc/rl-training/),
com venv proprio (`make venv-rl-training`) e as dependencias de treino
(`stable-baselines3`, etc.) declaradas la, nao aqui.

## Contrato de dados

**Observacao** (`spaces.Dict`): um item por campo numerico/booleano de
`domain::WorldView` -- posicao (`northM`/`eastM`/`altitudeM`), atitude
(`headingDeg`/`rollDeg`/`pitchDeg`), `speedKts`/`fuelFraction`/`mach`/
`gLoad`/`alphaDeg`, terreno (`terrainValid`/`terrainElevM`/`altitudeAglM`),
contato de radar (`hasContact` + `contactRangeM`/`contactRelBearingDeg`/...)
e alerta tatico (`hasAlert` + `alertRangeM`/...), mais `weaponReady`. Campos
de texto (`contactName`, `alertSender`, `alertContactName`) ficam de fora do
espaco de observacao -- disponiveis em `info["raw_state"]` para debug/log.

**Acao** (`spaces.Box(3,)`): `[headingDeg, altitudeM, speedKts]` -- os tres
campos de `domain::FlightCommand`, os unicos que `FlightAction::execute()`
de fato atua (via os hold-modes do `Autopilot`). Os efeitos colaterais
opcionais de `FlightAction` (transmitir alerta, lancar missil) ficam de fora
do v1.

**Reward**: NAO e calculado em C++ -- e escolha de pesquisa, nao
infraestrutura. `env.py` traz um `default_reward()` minimo (custo pequeno
por passo, penalidade grande se `terminated`), substituivel pelo parametro
`reward_fn` do construtor de `MixrFlightEnv`.

## Limites conhecidos e armadilhas confirmadas rodando

- **Um agente RL por processo.** Nem `RLBridgeBehavior` nem
  `shared/xrlbridge` tem chave por `playerId` -- ver o "porque" nos dois
  cabecalhos. Rodar mais de um `falcon*` com `RLBridgeBehavior` no MESMO
  cenario misturaria os comandos/observacoes dos dois.
- **`player_name` TEM DE bater com o player que o `.edl` configurou com
  `( RLBridgeBehavior )`** (default: `falcon1`, em
  `src/rl/configs/scenario_rl.edl`) -- BUG CONFIRMADO E CORRIGIDO: como a
  ponte nao tem chave por player id (item acima), `step()`/`reset()` sempre
  trocam Command/Observation com o player que o `.edl` escolheu, nunca com o
  `player_name` passado ao construtor. Antes da correcao, pedir um
  `player_name` diferente (typo, ou um player que existe mas nao e o
  configurado com `RLBridgeBehavior` -- ex.: `falcon2..4`) fazia
  `terminated` ficar preso em `False` para sempre, silenciosamente: a
  checagem de `isCrashed()` olhava um player sem nenhuma relacao com o
  Command/Observation reais. `NativeSimulation::reset()` agora falha com um
  erro claro se o player pedido nao existir no cenario; nao ha como validar
  daqui que e o MESMO player com `RLBridgeBehavior` -- esse tipo mora no
  plugin do modelo, que este host nao pode conhecer
  (`tests/guard/check_host_opaco.sh`).
- **SO PODE EXISTIR UMA `Station` POR PROCESSO -- confirmado, nao e mais
  hipotese.** `shared/xplugin` sela o registro de plugins depois do
  primeiro `edl_parser()`; um SEGUNDO `MixrFlightEnv()` no mesmo processo
  aborta no primeiro `reset()` dele com "loadModule(...) depois do parse".
  Precisa de mais de um cenario/episodio independente no mesmo run? Um
  processo por `MixrFlightEnv` (`multiprocessing` do lado Python), nao mais
  de um `Env` no mesmo processo. Ver o cabecalho de
  `src/rl/bindings/NativeSimulation.hpp`.
- **`reset()` REPETIDO NA MESMA instancia funciona, com uma deriva numerica
  pequena** -- confirmado rodando (`src/rl/tests/test_smoke.py`): chamar
  `reset()` varias vezes no MESMO `MixrFlightEnv` restaura posicao/
  combustivel bem proximos do valor inicial, com residuo de integracao do
  JSBSim na ordem de `1e-5` m / `1e-6` de fracao de combustivel entre uma
  chamada e outra -- irrelevante para RL, documentado para quem for
  depurar "por que o baseline nao bate byte a byte".
- **`import mixr_gym` TEM DE vir antes de `numpy`/`gymnasium` no seu
  script**, se voce importar esses pacotes por conta propria antes de
  `mixr_gym` (o pacote ja se protege internamente ao importar `numpy`/
  `gymnasium` DEPOIS de `._native`, mas isso so ajuda se `mixr_gym` for o
  PRIMEIRO a tocar essas bibliotecas no processo). Sem isso, a primeira
  chamada a `reset()` SEGFAULTA dentro de libstdc++ (dentro de um
  `std::cout` de `shared/xplugin/PluginRegistry.cpp`, ao carregar
  `libflight_tc.so`) -- confirmado rodando, tudo indica estouro do
  excedente de TLS estatico do glibc quando muitas extensoes C ja foram
  carregadas (numpy sozinho traz ~15) antes da nossa. `mixr_gym/__init__.py`
  documenta os dois experimentos que isolaram a causa. `src/rl/tests/
  test_smoke.py` importa `mixr_gym` antes de `numpy` de proposito, fora de
  ordem alfabetica -- e o padrao recomendado pra qualquer script que use
  este pacote.
- **Latencia de atuacao de um frame**: `step(action)` publica o comando
  ANTES de `tcFrame()`, mas a `Observation` devolvida e capturada no MESMO
  frame -- ou seja, reflete o efeito do comando do passo ANTERIOR (o atual
  so comeca a manifestar fisicamente no proximo `tcFrame()`). Comportamento
  padrao de qualquer problema de controle em malha fechada, nao um bug.
