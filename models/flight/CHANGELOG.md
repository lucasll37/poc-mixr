# Changelog — `flight`

Todo projeto de modelo deste repositório tem `tests/`, `docs/`, `README.md` e **este arquivo** —
a regra, e o porquê dela, estão em [../README.md](../README.md); a guarda
[`tests/guard/check_modelo_estrutura.sh`](../../tests/guard/check_modelo_estrutura.sh) a trava.

Formato adaptado de [Keep a Changelog](https://keepachangelog.com/pt-br/1.1.0/).

**A versão é a do `project()` em [meson.build](meson.build)** — hoje `1.0.0`. Não existe outra:
não há tag de git, e o descritor do plugin não carrega versão do modelo (`PluginDescV1` tem
`plugin_name`, `mixr_pkg_version` e `build_id`, e nada mais — ver
[`shared/xplugin/PluginAbi.hpp`](../../shared/xplugin/PluginAbi.hpp)). Subir a linha `version:`
do `meson.build` é o que "lançar" quer dizer aqui.

**As datas saem da data de COMMIT, nunca da mensagem** — todo commit deste repositório se chama
`up`, então a mensagem não serve para nada. As entradas anteriores à criação deste arquivo foram
reconstruídas do código e dos documentos, e por isso são grossas: uma linha por mudança que
alguém precisaria saber antes de mexer neste modelo, não uma por commit.

---

## [Não versionado]

### Adicionado

- **Inferência ONNX dentro da árvore de comportamento** — os nós `OnnxScore`
  (`bt/nodes/OnnxScoreCondition`) e `OnnxPolicy` (`bt/nodes/OnnxPolicyAction`), registrados por
  `bt/bt_factory_sdk.cpp`, mais a árvore de deploy `configs/flight_tree_onnx.xml` e o
  `configs/policy_example.onnx`. Fecha o ciclo de `src/rl`: até aqui a política só rodava com um
  processo Python **dirigindo** o frame de fora (`src/rl/bindings` + `ubf/RLBridgeBehavior`, uma
  caixa de correio com um frame de latência); o nó roda a MESMA política, exportada para `.onnx`,
  **dentro** do `genAction()` — lê o `WorldView` deste frame e comanda neste frame. A ordem dos 28
  campos de entrada é a ordem canônica de `xrlbridge/ObservationFields.hpp`, a mesma que o
  exportador usa: não há duas listas. **O `.onnx` publicado tem pesos ALEATÓRIOS** (`--random` do
  exportador) — existe para exercitar a cadeia inteira sem depender de uma sessão de treino; uma
  política de verdade substitui o arquivo sem recompilar nada. Degradação: modelo ausente, forma
  errada ou falha de inferência devolvem `FAILURE` sem comandar, e o `Fallback` da árvore cai no
  ramo escrito à mão — uma política que não carrega não tira a aeronave do ar. (2026-09-03)
- **`RLBridgeBehavior`** (`ubf/RLBridgeBehavior`) — o comportamento UBF que troca comando e
  observação com o host de RL por `shared/xrlbridge`. Mora aqui, e não num plugin à parte como o
  `missile`, porque `genAction()` precisa de `dynamic_cast<const xnative::FlightState*>` e de
  construir um `xnative::FlightAction` — tipos CONCRETOS deste modelo, e RTTI com visibilidade
  oculta não é confiável atravessando dois `.so`. **Preço, e é mecânico:** virou o 7º nome de
  `libflight.so` (8º de `libflight_tc.so`), e como `provides:` é igualdade EXATA de conjunto,
  todo cenário que carrega este plugin precisou de uma linha a mais — inclusive
  `models/fixtures/stub`, que roda o cenário de produção trocando só o `file:`. (2026-09-03)
- **Instrumentação de log** (`shared/xlog`) em `FlightAction::execute()` — o único ponto de
  atuação comum aos dois agentes: `INFO` na transição de comportamento, `WARNING` no alerta
  tático transmitido e nas duas formas de o lançamento não acontecer (alvo inexistente, cabide
  vazio), `ERROR` quando o ator não tem `Autopilot` (a decisão não podia ser atuada e isso era
  **mudo**), e `DEBUG` de batimento a cada 500 decisões. **`changedFor()` transforma estado
  contínuo em BORDA** — `execute()` roda até 50 Hz por aeronave e `broadcast` fica *ligado*
  durante toda a evasão: logar direto no `if (broadcast)` deu ~50 linhas/s por aeronave (medido:
  1856 linhas em 20 s, girando três vezes o buffer de 500 e engolindo justamente as transições).
  Depois: 26 linhas em 30 s do mesmo cenário. (2026-09-03)
- Este `CHANGELOG.md`. (2026-09-03)

### Mudado

- **`make install-host` deposita em `models/plugins/`, nunca mais em `dist/`** — o mesmo depósito
  que um `.so` de terceiro usa. Quem sincroniza `models/plugins/` → `dist/lib(share)/mixr-plugins/`
  é o alvo `sync-plugins` do `make install` da raiz. Compilar este modelo deixou de presumir onde
  o host guarda os artefatos dele: `dlopen()` só acontece em tempo de execução, então só RODAR
  algo precisa da união. (2026-09-03)

---

## [1.0.0] — 2026-09-02

O estado com que o modelo passou a existir como projeto próprio. Extraído em **2026-09-01** como
`models/flight-model/` e renomeado para `models/flight/` no dia seguinte.

### Adicionado

- **O modelo virou um projeto Meson INDEPENDENTE, carregado por `dlopen`** — deixou de ser alvo
  do host. Não é arrumação: é o que torna verificável o cenário de um terceiro entregar só o
  binário. Enquanto o modelo era alvo do host, o `files()` dele listava os `.cpp` daqui e o
  `meson setup` do host exigia este fonte — o oposto do que se queria provar. A guarda
  `tests/guard/check_host_opaco.sh` trava o invariante.
- **UMA árvore, DOIS artefatos**: `libflight.so` e `libflight_tc.so`, este com
  `-DFLIGHT_TC_AGENT`, que é o que liga o `FlightAgentTC` (o agente do pool de tempo crítico da
  poc `multi-thread`). Dissolveu por construção ~3.100 linhas duplicadas entre as duas pocs
  gêmeas, que antes eram sustentadas por um teste de guarda.
- **`data/jsbsim/` (o c310) passou a morar aqui** — é dado do MODELO, não do cenário: `domain/` e
  `bt/` são calibrados para esta aeronave (`maxClimbRateMps`/`maxRateOfTurnDps`, a folga do piso
  anti-CFIT, os limiares de combustível), então trocar de aeronave sem recalibrar o modelo já não
  faria sentido. Antes era a MESMA cópia byte-idêntica vendorizada três vezes, uma por poc.
- **Política de lançamento de armas**: `bt/nodes/LaunchEnvelopeCondition` e
  `bt/nodes/LaunchMissileAction`, mais a resolução de alvo e a liberação em
  `FlightAction::execute()`. Usados só pela árvore de demo `configs/flight_tree_missile_demo.xml`
  — a árvore de produção fica intocada, e os nomes publicados pelo plugin não mudaram (o míssil
  em si é o `models/missile`, um segundo `( PluginModule )`).
- **`FlightAction::execute()` escreve o `threadTag` no `xboard`** — antes só o `FlightAgentTC`
  contava, então a coluna de thread do `./app` ficava presa em `-` para quem decidisse no laço de
  background: faltava o DADO, não o destaque.
- As peças que faziam o projeto autocontido **à época** — o `CHANGELOG.md`, a quinta, é desta
  rodada: **`Makefile` autocontido**
  (configura, compila, testa e instala em `./dist`, sem chamar o Makefile da raiz), **`tests/`**
  (`domain`, `tree` e `native` — nenhuma levanta `Station`), **`docs/ARCHITECTURE.md`** (a
  calibração do c310 e as armadilhas deste modelo) e **`README.md`**.

### Neutralidade provada

Com o modelo fora do executável e carregado de `dist/`, o dump `frame=` das duas pocs saiu
**byte-idêntico** ao de antes de existir plugin nenhum.
