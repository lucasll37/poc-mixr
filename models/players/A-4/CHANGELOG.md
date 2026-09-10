# Changelog — `flight`

Todo projeto de modelo deste repositório tem `tests/`, `docs/`, `README.md` e **este arquivo** —
a regra, e o porquê dela, estão em
[`.claude/rules/models-plugin.md`](../../../.claude/rules/models-plugin.md); a guarda
[`tests/guard/check_modelo_estrutura.sh`](../../../tests/guard/check_modelo_estrutura.sh) a trava.

Formato adaptado de [Keep a Changelog](https://keepachangelog.com/pt-br/1.1.0/).

**A versão é a do `project()` em [meson.build](meson.build)** — hoje `1.0.0`. Não existe outra:
não há tag de git, e o descritor do plugin não carrega versão do modelo (`PluginDescV1` tem
`plugin_name`, `mixr_pkg_version` e `build_id`, e nada mais — ver
[`libs/xplugin/PluginAbi.hpp`](../../../libs/xplugin/PluginAbi.hpp)). Subir a linha `version:`
do `meson.build` é o que "lançar" quer dizer aqui.

**As datas saem da data de COMMIT, nunca da mensagem** — todo commit deste repositório se chama
`up`, então a mensagem não serve para nada. As entradas anteriores à criação deste arquivo foram
reconstruídas do código e dos documentos, e por isso são grossas: uma linha por mudança que
alguém precisaria saber antes de mexer neste modelo, não uma por commit.

---

## [Não versionado]

### Adicionado

- **Slow roll: um nó de árvore que faz a aeronave girar 360° em torno do eixo longitudinal, em
  instantes sorteados.** Quatro peças novas, uma por camada:
  - `domain::AerobaticPlan` (`include/domain/AerobaticPlan.hpp`) — regra pura, sem MIXR nem SDK,
    no molde de `domain::PatrolPlan`: `std::mt19937_64` privado, semente já derivada chegando de
    fora, e **sorteio num evento discreto** (o fim de uma manobra), nunca por `dt` — é o
    invariante que preserva o determinismo entre 1/2/4 threads.
  - `bt_nodes::SlowRollAction`, nome de fábrica BT `SlowRoll`. **Falha** enquanto não é hora de
    rolar, o que deixa pô-lo no topo de um `Fallback` sem sequestrar a árvore.
  - `configs/flight_tree_random.xml` — `( SlowRoll )` por cima de `( Navigate )`.
  - Quatro slots do `( BtBehavior )`: `slowRollMinInterval`, `slowRollMaxInterval`,
    `slowRollStick`, `slowRollTimeout`. **`slowRollStick` nasce 0, ou seja o recurso nasce
    desligado** — nenhum cenário existente mudou de comportamento.
  - `domain::FlightCommand` ganhou `rollOverride`/`rollStick`. Não toca o contrato de RL:
    `XRLBRIDGE_ACTION_FIELDS` enumera os campos por nome e continua com 3.
  - A semente é o **segundo consumidor** do mesmo `instanceSeed` de `patrolMasterSeed`, com salt
    de propósito próprio (`kSlowRollSalt`) — exatamente o caso que o comentário de
    `kPatrolJitterSalt` antecipava. Nenhum slot de semente novo.

  Cenário de demonstração: `sandbox/A4-6DOF-RANDOM` (as 8 aeronaves de `A4-6DOF`, cada uma
  rolando em instantes próprios).

### Alterado

- **Toda a aleatoriedade passou a ser tratada por `libs/xrandom`.** A lib tinha só a camada de
  DERIVAÇÃO de sementes (`fnv1a64`/`deriveSeed`); o gerador em si estava **duplicado** dentro de
  cada consumidor — `domain::PatrolPlan` e `domain::AerobaticPlan` tinham, cada um, o próprio
  `std::mt19937_64` privado e a própria `std::uniform_real_distribution`. Agora a lib expõe a
  classe `Rng`, e os dois a usam: um `grep` por `mt19937`/`uniform_real_distribution` fora de
  `libs/xrandom/` volta só comentários.

  **O que destravou:** o motivo de a duplicação existir era real — `domain/` é compilado por
  `test_domain`/`test_tree` **sem o SDK**, e o header só fica visível via `dist/include`. Mas ele
  é header-only e sem dependência nenhuma, então incluí-lo não arrasta MIXR: bastou dar aos dois
  alvos o **caminho de include**, nunca o link
  (`sdk_dep.partial_dependency(includes: true, compile_args: true)` em `tests/meson.build`).
  A propriedade "`test_tree` NÃO linka o MIXR" continua valendo — conferida com `ldd`: **zero**
  libs do MIXR nos dois binários.

  **Três decisões que estavam implícitas e agora estão escritas num lugar só**, cada uma com
  teste próprio em `tests/domain/test_xrandom.cpp`: a distribuição é construída a cada chamada e
  nunca guardada (uma `uniform_real_distribution` guardada tem estado próprio em algumas
  implementações, e ele sobreviveria a um `seed()` — resemear não voltaria ao início da
  sequência); faixa ou amplitude degenerada devolve o piso/zero **sem consumir o gerador** (é como
  um consumidor desliga a variação sem deslocar a sequência de quem ainda sorteia); e não existe
  gerador global nem construtor que invente semente (nada de `std::random_device`).

  **Refactor puro, provado:** o dump determinístico da fixture `intruder` de `flight` (3.000
  frames, 2 threads) sai **byte a byte idêntico** ao de antes da troca — a sequência de sorteios
  do jitter de patrulha não se moveu.

  Fora de escopo, e por quê: os três `np.random.default_rng(semente)` de
  `src/poc/onnx-policy/tools/train_policy.py`, `src/poc/rl-training/tools/export_onnx.py` e
  `src/rl/tests/test_smoke.py` continuam como estão — são numpy, offline, fora do frame de
  simulação, e já semeados explicitamente; `libs/xrandom` é um header C++ e não os alcança.


- **`data/jsbsim/aircraft/A4/a4ap.xml`: o nivelador de asas deixou de ser incondicional e passou
  a ser gateado em `ap/heading_hold == 1`.** Sem isso a acrobacia era **fisicamente impossível**,
  e o modo de falha era mudo. A conta, com aileron cheio: o comando líquido na superfície é
  `clip(ap/aileron_cmd + 1, ±1)` e ele **zera** quando `0,8·φ + 0,6·p = 1` — 71,6° de banco
  parado, e apenas ~50° a 0,5 rad/s. A aeronave travava de lado e nunca fechava um tonneau.
  Não há como resolver em C++: `JSBSimModel` é `final` com `fdmex`/`propMgr` `protected`.

  `ap/heading_hold` é o gate certo porque `FlightAction::execute()` o liga em **toda** decisão
  atuada do voo normal — então o nivelador segue exatamente como antes, e só sai do caminho no
  único momento em que atrapalha. **Medido (fixture `intruder` de `flight`, 3000 frames, 2
  threads):** todos os campos físicos do dump — posição, altitude, rumo, banco, arfagem,
  velocidade, combustível — saem **exatamente iguais** aos de antes do gate, e nenhum rótulo
  `bt=` divergiu. A única diferença em 120 linhas foram 14 valores de `trackRange` no **último
  dígito impresso** (~1e-9 em ~20 km, ou seja ~1e-13 relativo): o acréscimo de um nó `<switch>`
  ao grafo do FCS perturba o estado físico abaixo da precisão impressa, e o filtro alfa-beta do
  track manager amplifica isso até o último decimal. Confirmado que não é ruído de execução: duas
  execuções da MESMA configuração saem byte a byte idênticas.

  O **amortecedor de taxa** (`fcs/roll-rate-damper`, `-0,6·p`) continua incondicional, de
  propósito: fora da acrobacia ele segue segurando a divergência em espiral da célula; durante
  ela, é ele que limita a taxa de rolagem — é ele que faz o giro ser *slow*.

  **Escopo:** só o A-4. `models/players/C-130/.../c130ap.xml` tem cópia própria do nivelador
  (ganho `-0,2`) e não foi tocada; uma acrobacia lá exigiria o mesmo gate, à parte.

- **`ubf/FlightAction.cpp` ganhou um ramo de atuação por stick.** Duas armadilhas que ele existe
  para resolver: (1) o comando de stick é **pegajoso** do lado do JSBSim (o `FCS` guarda o último
  `SetDaCmd()` e nada o reaplica por frame), então o ramo normal zera explicitamente — o que cobre
  toda saída da manobra, inclusive a que não passa pelo nó; (2) o comando vai **no `Autopilot`,
  nunca no `AirVehicle`** — `Autopilot::headingController()` roda toda fase 3 e sobrescreve o
  dynamics model com o `stickRollPos` do próprio `Autopilot`, então um `AirVehicle::setControlStick()`
  seria zerado no frame seguinte. `setNavMode(false)` também é obrigatório: `modeManager()` chama
  `setNavMode(isNavModeOn())` toda fase 3, e `setNavMode(true)` religa os três hold modes.

### Medido

- **A manobra fecha os 360° em ~7,9 s** (≈46 °/s médios, com `slowRollStick: 0.9`) — não por
  timeout. Banco observado no dump chega a **±177°**, ou seja passa folgadamente da barreira de
  71,6° que existia antes do gate.
- **Cada giro custa ~530 m de altitude** (~1.700 ft), comparado contra o mesmo cenário sem
  acrobacia. É físico: um tonneau só de aileron não tem compensação de profundor, e invertido o
  *altitude hold* comanda o profundor no sentido contrário. Diminuir `slowRollStick` **piora**
  (giro mais longo = mais tempo perdendo altitude).
- **480 s, 8 aeronaves, 5 giros cada: nenhuma caiu nem congelou.** Pior AGL entre as oito:
  **564 m** (a aeronave mais baixa da pilha), contra ~1.539 m no mesmo cenário sem acrobacia — e
  ela recupera nas pernas de subida da rota. A altitude comandada durante a manobra continua
  passando por `clampAltitudeToTerrain()`, mas a aeronave **afunda abaixo do comandado** durante
  o giro: quem der intervalos curtos, ou voar mais perto do terreno, precisa refazer esta medição.
- Determinismo com 1, 2 e 4 threads T/C: dumps **byte-idênticos** (4000 frames), mais a repetição
  de 4 threads.

### Corrigido

- **`tools/update_bt_models.py` destruiria uma árvore cujo comentário MENCIONASSE
  `<TreeNodesModel>`** — defeito latente aqui, achado (e reproduzido, com perda real do arquivo)
  em `../template`, que ganhou uma camada `bt/` e escreveu esse comentário na árvore nova. A
  regex do bloco não conhece comentário: casa a partir da menção e, com `re.DOTALL`, engole o
  resto do comentário, o `-->`, o `<root>` e a `<BehaviorTree>` inteira — reportando
  "substituído", sem erro. Nenhuma das cinco árvores deste projeto menciona a tag em comentário,
  então nada aqui estava quebrado; a correção (mascarar os comentários antes da busca, mesma
  técnica de `tools/mixr_source_scan.py::mask_source()` na raiz) foi aplicada às duas cópias do
  script para não deixar a armadilha esperando a próxima árvore. Conferido: as cinco árvores
  saem byte-idênticas depois do fix.

### Adicionado

- **Cobertura de teste direta para `OnnxScoreCondition`/`OnnxPolicyAction`**
  (`tests/native/test_onnx_nodes.cpp`, 9 casos) — achado por auditoria: o comentário de
  `tests/meson.build` já afirmava "estes nós são exercitados pelo test_native", mas nenhum teste,
  em lugar nenhum do repositório, de fato construía/tickava essas duas classes. A cobertura real
  vinha só de `scenario-policy-onnx` (o binário completo, caminho feliz com um `.onnx` de pesos
  aleatórios) — prova a cadeia inteira, mas não isola porta `model` ausente, caminho inexistente,
  ou `index` fora da faixa de saídas do modelo. Reusa o mesmo `configs/policy_example.onnx` de
  pesos aleatórios que `scenario-policy-onnx` já usa (forma certa, 28→3; pesos irrelevantes pros
  casos testados). Teste do bounds-check desenhado para não depender de UB (lê dentro do próprio
  buffer `std::array<float,16>` do nó, num índice nunca escrito pela inferência real, com
  `above=false` — sem o bounds-check o resultado vira SUCCESS, não FAILURE por coincidência;
  confirmado revertendo o bounds-check e rodando o teste). (2026-09-08)
- **Gerador do `<TreeNodesModel>` para o Groot** (`tools/dump_tree_model.cpp`, alvo Meson
  `dump-tree-model`, + `tools/sync_tree_models.py`) — monta a MESMA `BT::BehaviorTreeFactory` que
  `xnative/factory.cpp` registra (`bt_nodes::registerNodes()` + `registerSdkNodes()`) e usa
  `BT::writeTreeNodesModelXML()`, nativa do BT.CPP, para gerar o bloco que o Groot precisa para
  reconhecer nós customizados — substitui mantê-lo à mão nos 5 `flight_tree*.xml`. Dois modos: sem
  argumento imprime só o fragmento (o que `sync_tree_models.py` usa para resincronizar os 5 XMLs
  de produção de uma vez); `--skeleton [ID]` imprime um `.xml` completo, pronto para abrir no
  Groot, com uma árvore vazia + a paleta populada. Teste novo, `tree-model-sync` (suíte `tree`),
  acusa quando os XMLs de produção divergem do que a factory de fato registra — sem ele, um nó
  novo sem resincronizar era um "verde silencioso" até alguém tentar abrir a árvore no Groot. Ver
  [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md). (2026-09-07)
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
  observação com o host de RL por `libs/xrlbridge`. Mora aqui, e não num plugin à parte como o
  `missile`, porque `genAction()` precisa de `dynamic_cast<const xnative::FlightState*>` e de
  construir um `xnative::FlightAction` — tipos CONCRETOS deste modelo, e RTTI com visibilidade
  oculta não é confiável atravessando dois `.so`. **Preço, e é mecânico:** virou o 7º nome de
  `libflight.so` (8º de `libflight_tc.so`), e como `provides:` é igualdade EXATA de conjunto,
  todo cenário que carrega este plugin precisou de uma linha a mais — inclusive
  `models/players/fixtures/stub`, que roda o cenário de produção trocando só o `file:`. (2026-09-03)
- **Instrumentação de log** (`libs/xlog`) em `FlightAction::execute()` — o único ponto de
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

- **O diretório PAI passou de `models/player/` para `models/players/`** — afeta os quatro
  projetos ali dentro (`A-4`, `missile`, `fixtures/stub`, `template`) igualmente; este modelo
  passou a morar em `models/players/A-4/`. `git mv` preservou o histórico. Mesma varredura da
  entrada anterior (rename de `A4` para `A-4`, logo abaixo): substituição textual em bloco para
  `models/player/` → `models/players/`, mais os pontos que uma busca por essa substring contígua
  não alcança — dois caminhos montados por concatenação em `tests/meson.build` (`raiz / 'models' /
  'player' / ...`) e um em `tests/guard/check_colisao_fabrica.py` (`REPO_ROOT / "models" /
  "player"`).
  **Armadilha confirmada, não hipotética — a substituição em bloco quebrou o build na primeira
  passada.** `"models/player"` é substring de `#include "mixr/models/player/Player.hpp"` — o
  caminho de header do MIXR **vendorizado** (`mixr::models::player`, o agrupamento interno do
  framework para classes de veículo: `player/air/`, `player/weapon/`, `player/effect/...`),
  sem relação nenhuma com este diretório. A troca em bloco pluralizou ~34 `#include`s (todo
  arquivo deste repositório que inclui `Player.hpp`/`AirVehicle.hpp`/etc.) mais três comentários
  que citam o caminho vendorizado por extenso (`libs/xplugin/PluginAbi.hpp`,
  `models/players/missile/src/xmissile/GuidedMissile.hpp`) e um caminho funcional em
  `src/ui/scripts/generate_edl_catalog.py` (`extract_primary_components()`, que lê
  `contexts/src/mixr/src/models/player/Player.cpp` para extrair os papéis primários de
  `Player::updateSystemPointers()`) — o sintoma neste último foi um teste falhando
  (`tests/tools/test_edl_catalog.py`), não um erro de compilação. Só foi pego porque **todo**
  o pipeline foi rebuildado e testado de ponta a ponta depois do rename (`make build` falhou com
  `mixr/models/players/Player.hpp: No such file or directory` na primeira tentativa) — uma
  substituição textual "só documentação" não teria disparado esse alarme. `tools/
  extract_execution_chain.py::category_of()` usa a MESMA distinção (compara contra a string
  `"player"` para agrupar arquivos de `contexts/src/mixr/src/models/`) mas não foi afetado porque
  ali o caminho já vinha por segmentos (`parts.index("models")`), não como substring contígua.
  As entradas históricas abaixo (sobre o rename `A4`→`A-4` e os dois renames anteriores) continuam
  dizendo `models/player/...` — é o nome que o diretório pai tinha NAQUELE momento. (2026-09-07)
- **A pasta deste modelo passou de `models/player/A4/` para `models/player/A-4/`** — o nome do
  MODELO (o que aparece em título/prosa nos documentos, em `models/REGISTRO.md`, e agora também no
  diretório) passa a ser escrito `A-4`, o mesmo hífen da designação real da aeronave (Douglas
  A-4 Skyhawk, ver a entrada de 2026-09-05 abaixo). Só o título do diretório-fonte e as
  referências de PROSA/caminho mudaram — nome de fábrica (`MIXR_PLUGIN_DEFINE("flight", ...)`),
  bibliotecas (`libflight.so`/`libflight_tc.so`), destino de instalação
  (`dist/share/mixr-plugins/flight/`) e o identificador JSBSim da aeronave (`data/jsbsim/
  aircraft/A4/`, `model: "A4"` nos `.edl`/`.edl.in`) continuam **`flight`**/**`A4`** — o
  identificador JSBSim é um namespace À PARTE (a pasta de dados da aeronave, não o modelo/plugin
  em si) e deliberadamente não foi tocado: renomeá-lo exigiria migrar `model: "A4"` em toda
  fixture/cenário de produção que carrega este plugin, escopo bem maior do que "o nome do modelo".
  `git mv` preservou o histórico do diretório; ~120 arquivos com referências de CAMINHO
  (`models/player/A4` → `models/player/A-4`) foram atualizados em bloco, mais os pontos onde um
  caminho era montado por CONCATENAÇÃO de segmentos em vez de string contígua — dois em
  `tests/meson.build` (`raiz / 'models' / 'player' / 'A4' / ...`, invisível a uma busca textual por
  `player/A4`) — e o rótulo de origem `"plugin:A4"` que `src/ui/scripts/generate_edl_catalog.py`
  deriva do nome do diretório (`tests/tools/test_edl_catalog.py`, que fixa esse rótulo no teste).
  As DUAS entradas históricas abaixo (2026-09-01/02 e 2026-09-05) continuam dizendo `A4` — é o
  nome que a pasta tinha NAQUELE momento, não um erro de digitação. (2026-09-07)
- **O `Makefile` autocontido deste diretório passou a seguir o padrão do Makefile raiz do
  `poc-mixr`**: o alvo `all` (que era um atalho redundante para `build`, e a única razão de
  `make` sozinho não cair em `help`) foi removido — `.DEFAULT_GOAL := help` já garantia o
  comportamento, mas o `all` sobrando deixava a intenção ambígua. `make` sem argumento agora só
  lista os alvos disponíveis, nunca compila por acidente. Os alvos foram reordenados no arquivo
  para seguir a sequência canônica (`clean` → `check-root` → `configure` → `build` → `test` →
  `install` → `install-host` → `uninstall-host` → `help`), e `make check-root` ganhou feedback
  positivo (`check-root: OK -> SDK do host publicado em <ROOT>/dist`, em verde) além do negativo
  que já existia — antes, rodar `make check-root` com o pré-requisito satisfeito não imprimia
  nada, silêncio que não distingue "passou" de "não rodou". `README.md`/`CONTRIBUTING.md` deste
  modelo foram revisados junto, e o `INSTALL.md` (redundante com o `README.md`/`CONTRIBUTING.md`
  da raiz) foi removido. (2026-09-07)
- **A aeronave trocou de Cessna 310 (c310) para Douglas A-4 Skyhawk**, com dados aerodinâmicos
  em `data/jsbsim/aircraft/A4/` (originados de `shared/data/jsbsim/aircraft/A4/`, a base
  Aeromatic vendorizada na raiz do repositório). `data/jsbsim/aircraft/A4/a4ap.xml` é um
  `<autopilot>` JSBSim escrito para esta PoC-mixr — o A-4 de fábrica não vem com nenhum, e
  `JSBSimModel` só amarra `ap/heading_hold`/`ap/altitude_hold`/`ap/airspeed_hold` quando essas
  propriedades já existem no modelo (mesmo papel que `c310ap.xml`, agora removido, cumpria).
  `model:`/`type:` nos `.edl`/`.edl.in` de todos os cenários passaram de `"c310"`/`"C310"` para
  `"A4"`, as velocidades comandadas (`patrolSpeed`/`rtbSpeed`/`evadeSpeed`/`supportSpeed`) subiram
  para a faixa do A-4 (~350-430 kt, contra ~160-190 kt do c310), e o `modelMap` do Tacview em cada
  cenário passou de `"F-16C"` (uma skin de apresentação sobre a dinâmica do c310, ver o comentário
  antigo removido) para `"A-4E"` — a partir de agora a dinâmica E a visualização são a mesma
  aeronave. **Limite conhecido, documentado no cabeçalho de `a4ap.xml`**: os coeficientes
  látero-direcionais do dado Aeromatic (`Clb=-0.1 Cnr=-0.15 Clr=0.15 Cnb=0.12`) violam o critério
  clássico de estabilidade em espiral, e `JSBSimModel::reset()` não roda `FGTrim` — a aeronave
  nasce destrimada e diverge em poucos segundos sem um SAS (amortecedores de taxa em
  rolagem/arfagem + nivelador de asas, todos sempre ativos, não gateados por `ap/heading_hold`).
  Com o SAS, o determinismo (`check-single-thread`/`check-multi-thread`, 1/2/4 threads) passa
  byte-a-byte, mas o voo ainda deriva lentamente (dezenas de segundos, numa perna reta e longa)
  antes de o nivelador reafirmar o controle — escolhido deliberadamente sobre a alternativa (um
  nivelador forte o bastante para nunca derivar também impede a aeronave de completar curvas
  comandadas, o que quebraria PATROL/EVADE/RTB de verdade). Recalibrar o SAS, ou reexportar a
  aeronave com `Clr`/`Cnb` ajustados, fica registrado como trabalho futuro. (2026-09-05)
- **A pasta deste modelo passou de `models/player/flight/` para `models/player/A4/`** — só o
  título do diretório-fonte; o nome de fábrica do plugin (`MIXR_PLUGIN_DEFINE("flight", ...)`),
  as bibliotecas (`libflight.so`/`libflight_tc.so`) e o destino de instalação
  (`dist/share/mixr-plugins/flight/`) continuam `flight`, então nenhum `.edl`/`.edl.in` de
  cenário precisou mudar por causa do rename em si (só pela troca de aeronave, acima). Makefile
  raiz, `tests/meson.build`, `tests/guard/check_modelo_fresco.sh` e
  `tests/plugin/check_hotswap_rebuild.sh` tinham o caminho antigo hardcoded e foram atualizados.
  (2026-09-05)
- **`make install-host` deposita em `models/plugins/`, nunca mais em `dist/`** — o mesmo depósito
  que um `.so` de terceiro usa. Quem sincroniza `models/plugins/` → `dist/lib(share)/mixr-plugins/`
  é o alvo `sync-plugins` do `make install` da raiz. Compilar este modelo deixou de presumir onde
  o host guarda os artefatos dele: `dlopen()` só acontece em tempo de execução, então só RODAR
  algo precisa da união. (2026-09-03)

### Corrigido

- **RTB/SUPPORT/PATROL passam a respeitar o piso anti-CFIT** — antes, só
  `domain::ThreatPolicy::breakCommand()` aplicava `domain::clampToTerrain()`. `RtbPlan` é
  geometria pura sem noção de terreno (`rtbAltitude` é um valor FIXO do EDL, calibrado contra o
  pico do PRÓPRIO circuito de cada falcon, não contra o caminho de volta até a base, que cruza
  relevo diferente); `SupportAlertAction` comandava a altitude ABSOLUTA de um contato reportado
  por OUTRO player, sem validação nenhuma. Como o cenário de produção está "SEM ÁRBITRO" (sem
  `AltitudeSafetyBehavior` por cima), `ThreatPolicy` tinha virado a ÚNICA proteção ativa — os
  outros ramos só estavam seguros por coincidência de calibração manual, não por garantia em
  runtime. `bt_nodes::DecisionContext` ganhou um 9º getter, `clampAltitudeToTerrain(altitudeM)`;
  `ReturnToBaseAction`/`SupportAlertAction`/`PatrolAction` chamam o clamp antes de
  `decision().take()`. Decisão de escopo: **não** clampado dentro de `decision().take()` (o
  despacho universal de todo nó, inclusive `OnnxPolicyAction`/`PyDecideAction`) — mudaria o
  comportamento já documentado da política ONNX ("tem a última palavra sobre altitude") sem
  pedido pra isso. 3 testes novos em `test_flight_tree.cpp` provam que o clamp de fato ENTRA em
  ação (terreno alto o suficiente pra violar a altitude configurada). (2026-09-08)
- **`BtBehavior::buildTree()` nunca resetava `btFactory` entre chamadas.** `reset()` zera
  `treeBuilt` (permitindo um segundo `buildTree()` na mesma instância) mas nunca zerava
  `btFactory`; `BT::BehaviorTreeFactory::registerBuilder()` lança `BehaviorTreeException` pra
  qualquer ID já registrado — um segundo `registerNodes()`/`registerSdkNodes()` lançaria no
  primeiro nó, fora do `try/catch` (que só cobre `createTreeFromFile()`), propagando sem
  tratamento. `btFactory` agora é reatribuída a cada `buildTree()` — mesmo padrão já usado pra
  `tree` em `reset()`. Mecanismo confirmado por teste direto (`BtFactoryRegistration`,
  `test_xnative.cpp`); **não** reproduzido hoje via `reset()`+`step()` repetidos em `src/rl`
  (investigado rodando: `BtBehavior::reset()` parece nunca ser chamado uma segunda vez pelo
  cascade do `UbfArbiter` nativo) — cautela defensiva, não correção de um crash observado.
  (2026-09-08)
- **`NavigateAction` não reiniciava a suavização de rumo depois de um gap de guiagem inválida.**
  `hasCommandedHeading_`/`commandedHeadingDeg_` ficavam congelados no último valor quando
  `hasNavSteering` virava falso (ex.: a árvore troca pra EVADE e depois volta pra NAV —
  `full-systems-nav` é o único consumidor hoje); o próximo tick com guiagem válida caía no ramo
  de suavização em vez do de "primeiro tick", corrigindo pela taxa limitada a partir de um rumo
  antigo sem relação com a nova marcação. Corrigido zerando `hasCommandedHeading_` no próprio
  ramo de falha. Teste novo (`GapDeGuiagemInvalidaReiniciaASuavizacaoNaProximaBearing`,
  `test_flight_tree_nav.cpp`) confirma a regressão (falha com o código antigo — comando fica
  perto de 42° em vez de saltar pra 200°) e passa com o fix. (2026-09-08)
- **`FlightAction::execute()` sem null-check no nome do player** — `base::Identifier::getString()`
  devolve ponteiro cru, `nullptr` para um nome nunca atribuído; `FlightState::updateState()` já
  tratava isso, `FlightAction.cpp` não replicava nos 4 pontos de `LOG(...)`. Nenhum player de
  produção deste repositório é anônimo hoje — consistência com o padrão já escrito, não resposta
  a um crash observado. (2026-09-08)
- **`RLBridgeBehavior` aplicava um comando zerado/obsoleto no frame de priming de cada
  episódio.** `NativeSimulation::reset()` dispara `primeStation()` (RESET_EVENT + `tcFrame()` de
  aquecimento), que já chama `genAction()` — antes de qualquer `step()`/`setPendingCommand()` do
  lado Python. `xrlbridge::getPendingCommand()` nesse momento devolvia um `Command{}` default (a
  primeira vez) ou o ÚLTIMO comando do episódio ANTERIOR (resets seguintes, mesmo processo) — os
  dois indistinguíveis de "o host publicou isto de propósito". Medido: `falcon1: -- -> RL
  (hdg=0deg alt=0m vel=0kt)` como primeira decisão de todo episódio. `xrlbridge::Command` ganhou
  um flag `valid` (só `PyBindings.cpp::step()` o liga); `reset()` invalida o comando pendente
  ANTES de qualquer `primeStation()`; `genAction()` devolve `nullptr` quando o comando pendente
  não é válido, deixando o `AltitudeSafetyBehavior` do mesmo `UbfArbiter` decidir sozinho.
  Confirmado depois do fix: primeira decisão sai com valores de voo plausíveis
  (`hdg=278.6deg alt=3511m vel=343kt`). (2026-09-08)
- **`domain::PatrolPlan::advance()` só trocava UMA perna por chamada.** Um `dt` que cobrisse duas
  ou mais fronteiras de perna na mesma chamada (passo de controle grande, ou `legSeconds`
  configurado pequeno) deixava `legTimeRemaining()` negativo, e a folga só era recuperada aos
  poucos, uma chamada por vez, até o acúmulo (`legTimer_`) cair de volta abaixo de `legSeconds_`.
  Virou um `while`, e não um `if`: uma troca por fronteira cruzada, no mesmo `advance()`. Cobertura
  nova em `tests/domain/test_PatrolPlan.cpp` (um `dt` cobrindo múltiplas pernas) e dois testes de
  borda em `test_ThreatPolicy.cpp`/`test_flight_tree.cpp` (altitude igual conta como "acima"; o
  limiar exato de combustível da árvore de decisão). (2026-09-04)

---

## [1.0.0] — 2026-09-02

O estado com que o modelo passou a existir como projeto próprio. Extraído em **2026-09-01** como
`models/player/A4-model/` e renomeado para `models/player/A4/` no dia seguinte.

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
  em si é o `models/players/missile`, um segundo `( PluginModule )`).
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
