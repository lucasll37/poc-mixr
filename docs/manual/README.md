# `docs/manual/` — explorador de execução, EDL e classes built-in do MIXR

Página estática (React embutido, zero rede para abrir) com seis visões sobre o framework,
**curadas, não instrumentadas** — sem processo MIXR rodando por trás, herança, nome de fábrica,
registro, slots, fases e os trechos de código (com arquivo e linha reais) vêm de
`tools/generate_manual_catalog.py` (que reaproveita `tools/mixr_source_scan.py` e
`tools/extract_execution_chain.py` como módulos) escaneando o fonte de verdade
(`contexts/src/mixr/` e `models/players/A-4/`) — nada digitado à mão.

Todo bloco de código C++ das seis abas (Execução, Comportamento, step-by-step, Referência,
Catálogo) passa
por `cppTokenizeLines()`/`renderCppSrc()` (`doc.jsx`) — um highlight de sintaxe LEVE, por
heurística (não um lexer C++ de verdade), com sete cores fixas (`--cpp-*`, não redefinidas por
tema, calibradas para o fundo escuro do bloco de código, que não muda de tom entre claro/escuro):
palavra reservada, string/char, número, comentário, identificador `TODO_MAIUSCULO` (cobre macro
de verdade e constante de enum com a mesma cor — o que importa visualmente é "isto é uma
constante nomeada do framework"), chamada de função e qualificador de namespace/classe. Blocos de
texto EDL (a prévia de cenário na aba Execução/Comportamento, e os dois passos de dado na aba
step-by-step) não passam por ele de propósito — a heurística é calibrada para C++, coloriria
sintaxe EDL errado.

## Como se usar

```bash
make open-docs   # regenera (catalogo + index.html) e abre -- nenhum servidor;
                 # Linux nativo (xdg-open) e WSL2 (wslview/explorer.exe) pelo
                 # mesmo alvo, ver scripts/open_browser.sh
```

A primeira execução baixa React/ReactDOM UMD + Babel para `docs/manual/.cache/` (gitignored,
precisa de rede liberada para `cdnjs.cloudflare.com`/`registry.npmjs.org`); as próximas rodam
offline.

**Execução** (o botão na página diz "Simulação" — "Execução" aqui é só o nome descritivo do
conteúdo, mesma convenção do `CLAUDE.md`) — o ciclo de fases do MIXR (dynamics/transmit/receive/
process + as duas threads de decisão/fundo) desenhado sobre a árvore de um `( Aircraft )` com os
dez sistemas primários que
`Player::updateSystemPointers()` resolve por tipo (~72 nós, o mesmo cenário exaustivo de
`tests/fixtures/built-in_mixr_1`). Grafo navegável (pan/zoom/arrastar, timeline com transporte,
tema claro/escuro); clicar num nó fixa um popup com nome de fábrica/registro/contagem de slots;
clicar num quadradinho de fase pula direto pro passo em que aquele nó roda naquela fase.

**Comportamento** — a cadeia de decisão real de produção, `FlightAgentTC → Agent::controller →
BtBehavior → flight_tree.xml → FlightAction` (sem árbitro: `( UbfArbiter )`/
`( AltitudeSafetyBehavior )` foram removidos do cenário de produção — ver a seção "SEM ARBITRO" em
`src/poc/dis/flight/configs/scenario.edl.in`), através dos cenários que a usam. Ao contrário das
outras duas abas, é um "ensaio" escrito **à mão** — não
extraído automaticamente do fonte — então é a única das três sujeita a envelhecer em silêncio se
a cadeia de decisão mudar sem que este texto acompanhe.

**step-by-step** — da declaração `.edl` do envelope de disparo até o `event(KILL_EVENT, ...)` do
lado do alvo, 35 passos em ordem de execução, em 7 estágios (contexto → decisão → liberação →
transição → guiagem → detonação → epílogo). Cada passo com código real, syntax-highlighted
(keywords, strings, números, comentários, macros/constantes `TODO_MAIUSCULO`, chamada de função e
qualificador de namespace — sete cores fixas, calibradas para o fundo escuro do bloco de código,
que não muda de tom entre os dois temas da página) — modelo A-4/míssil, fonte nativo em
`contexts/src/mixr/`, ou o próprio `.edl` do cenário (sem highlight — a heurística é calibrada
para C++, não para EDL) — e uma nota sobre o que aquele passo emite, ou deixa de emitir. Cenário
de referência: `sandbox/A4-6DOF-MISSILE` (`a4_shooter` detecta `a4_target` pelo radar e dispara um
`( GuidedMissile )`). Mesma natureza da aba Comportamento — curadoria **à mão** sobre código real,
não extração automática — só que atravessando dois plugins (`models/players/A-4` +
`models/players/missile`), classes abstratas do MIXR (`AbstractWeapon`, `StoresMgr`) que o
Catálogo não cobre (ele só lista classe **concreta**, despachada por fábrica — ver abaixo), e a
matemática pura de guiagem/espoleta (`domain::proportionalNavigation`/`domain::proximityFuze`,
`domain::inLaunchEnvelope`) que nem é MIXR nem é do plugin em si. O prólogo (estágio "contexto")
liga as pontas com a aba Comportamento — reaproveita os MESMOS snippets extraídos de
`FlightAgentTC::controller`/`Agent::controller`/`FlightState::updateState` (`SNIPPETS`/
`FLIGHT_SNIPPETS`, a cadeia de fallback de `missileSnip()`), sem duplicar fonte nenhum. Mostra
nuances medidas lendo o fonte, não supostas: o alvo designado sempre recebe `KILL_EVENT` garantido
(o alcance de detonação nunca é setado por este míssil, cai no default `0.0`, sempre abaixo do
alcance letal); `Player::killedNotification()` só muda o `mode` para `KILLED` se o slot
`killRemoval` (default `false`) estiver ligado — sem ele, o alvo "morto" continua voando
normalmente; os dois REID (Recorder Event ID — o token que identifica cada evento gravável do
`mixr::recorder`, ver `dataRecorderTokens.hpp`) relevantes (`REID_WEAPON_RELEASED`/
`REID_PLAYER_KILLED`) nunca chegam ao
Tacview neste cenário, filtrados pelo `enabledList` do `dataRecorder`; e um míssil que "morre de
velhice" (`AbstractWeapon::updateTOF()`, fim de tempo de voo) nunca chama
`checkDetonationEffect()` — sem risco de dano, ao contrário do caminho de acerto/falha por
proximidade. Play/pause, `←`/`→`, clique em qualquer passo da lista lateral.

**Referência** — enciclopédia CURADA de classes built-in, complementar ao Catálogo (que é flat e
automático): poucas classes escolhidas a dedo, cada uma com recursos didáticos que o Catálogo não
tenta oferecer — animação, gráfico, diagrama de estados. Começou com uma única classe,
`mixr::models::Missile`; hoje tem quatro entradas na barra lateral (a lista continua desenhada para
crescer — o próximo espaço aparece marcado como "+ mais em breve"):

- `mixr::models::Missile` (o míssil ar-ar GENÉRICO, herdado por qualquer subclasse concreta —
  `( AamMissile )`, `( Sam )` — que não sobrescreva a guiagem).
- `mixr::models::Steerpoint` **+** `mixr::models::Route` (o waypoint e o sequenciador que anda
  entre eles — documentados juntos, na mesma página, porque um `Route` sem `Steerpoint` não navega
  nada e vice-versa).
- `mixr::models::Navigation` (com uma nota sobre `Ins`/`Gps`, que neste fork são a MESMA classe,
  sem override nenhum).
- `mixr::models::Autopilot` (o piloto automático nativo — `navMode`, os três "hold mode", e um
  achado central: os cinco slots de limite de manobra do Autopilot não têm efeito nos dois
  `DynamicsModel` shipped neste fork).

Um card de cabeçalho fixo (nome, fábrica, categoria, cadeia de herança, link "Ver no Catálogo →")
fica sempre visível; o conteúdo em si é dividido em **quatro sub-abas** (mesmo idioma visual de
`.mx-dtabs`/`.mx-dtab` que a aba Simulação já usa para Passo/Código/EDL/Classe — reaproveitado, não
reinventado), pra não empilhar tudo numa rolagem só:

- **Visão geral** — os quatro passos de "como a guiagem decide", em prosa curta, lado a lado com a
  tabela do `enum Detonation` (o resultado — separado do `mode`, que só diz DETONATED, nunca "por
  quê").
- **Laboratório de guiagem** — o coração da aba: um laboratório interativo que roda, em JS puro, a
  tradução LITERAL de `Missile::weaponGuidance()`/`weaponDynamics()` (guiagem nativa por **ponto de
  interceptação**, diferente da navegação proporcional do `( GuidedMissile )` deste repositório —
  ver step-by-step) sobre um alvo em linha reta. Uma pílula de status (EM VOO / ACERTO / FALHA)
  identifica o desfecho num relance; dois controles (rumo e velocidade do alvo) recalculam a
  simulação inteira; a animação top-down (SVG, viewBox auto-ajustado pra nunca cortar a
  trajetória) e o gráfico de alcance×tempo (marcando o instante exato da detonação) ficam lado a
  lado com uma leitura em tiles tipo HUD (alcance, taxa, velocidade, rumo atual, rumo comandado) e
  uma versão COMPACTA do diagrama de ciclo de vida que acende ao vivo, sincronizada com o quadro
  corrente — não é um enfeite parado, é o mesmo `mode` observável mudando de ACTIVE pra DETONATED
  no instante exato em que a espoleta dispara.
- **Slots** — os 8 slots próprios, com o que cada um faz de verdade — achado documentando, não
  suposto: `speedMaxG` tem slot, getter e participa de `copyData()`, mas **nunca é lido** dentro de
  `weaponGuidance()`/`weaponDynamics()` (confirmado por grep) — o G máximo de verdade é sempre a
  constante `maxG`, nunca escalado pela velocidade atual apesar do nome parecido.
- **Código-fonte** — o construtor (os defaults usados no laboratório) e os dois métodos nativos na
  íntegra, com highlight de sintaxe, mais uma observação NÃO confirmada rodando, só lida no fonte
  (marcada como tal, não como fato): `weaponDynamics()` usa `base::ETHG` (32,16 — pés/s²) onde a
  velocidade do míssil está documentada em m/s nos próprios comentários do slot — se os dois se
  misturam sem conversão, a taxa de giro nativa sairia ~3,28× maior do que o pretendido, a mesma
  razão que levou o `( GuidedMissile )` local a usar `base::ETHGM` (a versão já convertida) em vez
  de `base::ETHG`. O laboratório reproduz o valor LITERAL do código nativo (sem "corrigir" nada),
  então é essa taxa de giro que se vê animada na sub-aba anterior.

**Referência — Steerpoint/Route, Navigation, Autopilot** — mesmo padrão de hero + sub-abas da
página de Missile, cobrindo a cadeia completa de navegação nativa: `Steerpoint` (dado do waypoint)
→ `Route` (sequenciador) → `Navigation` (agregador, repassa o que o `Route` já calculou) →
`Autopilot` (consumidor, quando `navMode` está ligado). Achados medidos lendo o fonte, não
supostos:

- **`Route::autoSequencer()`** só avança um steerpoint quando DUAS condições valem no mesmo
  frame — distância dentro de `autoSeqDistance` **e** a marcação relativa já passou de ±90° (a
  aeronave já está indo embora do ponto, não só perto dele) — nunca um raio simples.
- **`Route::triggerAction()` roda ANTES de `incStpt()`** — a `Action` do steerpoint dispara
  enquanto o "to" ainda é esse mesmo ponto, contradizendo o comentário do próprio slot table
  nativo ("the 'to' steerpoint will have sequenced ... when action is triggered").
- **`Steerpoint.sca`/`isWarnSCA()`** é escrito a cada `compute()` mas não tem NENHUM chamador em
  todo o fork nem neste projeto (grep confirma) — slot morto para efeito de comportamento, apesar
  de calcular um valor de verdade. `Steerpoint.next` é ainda mais simples: nunca é lido, só
  escrito.
- **`Ins`/`Gps` são a MESMA classe que `Navigation`**, sem overrides — 0 slots próprios, 0 métodos
  sobrescritos nas duas. Trocar o nome de fábrica no `.edl` não muda nenhum comportamento neste
  fork.
- **`Autopilot::modeManager()` re-latcha `navMode` incondicionalmente**, todo frame, antes de
  decidir qual dos quatro modos (follow-the-lead/loiter/nav) processar — é por isso que desligar
  só um hold mode individual não "gruda": o próximo frame reimpõe o que `navMode` mandar.
  `processModeNavigation()` é perseguição pura (`setCommandedHeadingD(marcação bruta)`, todo
  frame, sem filtro).
- **Os cinco slots de limite de manobra do Autopilot são decorativos nos dois `DynamicsModel`
  deste fork**: `maxRateOfTurnDps`/`maxBankAngle` (2º/3º parâmetro de
  `setCommandedHeadingD()`) e `maxClimbRateMps`/`maxPitchAngle` (idem, de `setCommandedAltitude()`)
  chegam SEM NOME em `RacModel`/`JSBSimModel` — descartados em tempo de compilação;
  `maxAcceleration` (2º parâmetro de `setCommandedVelocityKts()`) chega COM nome (`vNps`) nos
  dois, mas nenhum corpo o referencia — descartado em runtime. O próprio cabeçalho de
  `Autopilot.hpp` já avisa que isso pode acontecer ("these inputs will have no effect" se o
  `DynamicsModel` não suportar) — aqui é o caso medido, não hipotético.
- **Laboratório de navegação** (sub-aba de Autopilot): usa os 4 waypoints REAIS de
  `tests/fixtures/full-systems-nav/configs/scenario_full_nav.edl.in` (coordenadas, velocidades e
  `autoSeqDistance: 1.5 NM`, todos valores do arquivo, não inventados) para comparar, lado a lado,
  o rumo comandado por `Autopilot::processModeNavigation()` (salta até 180° instantaneamente no
  momento do sequenciamento — perseguição pura, sem filtro) contra o de `NavigateAction.cpp`, a
  resposta que este projeto escreveu para o problema (rampa nunca acima de 3°/s — o
  `kMaxHeadingRateDegPerSec` do próprio nó, medido: salto máximo de exatamente 1,5°/quadro, o
  teto teórico com `dt=0,5s`). O laboratório demonstra o SALTO DISCONTÍNUO no comando — não a
  divergência completa (~200s até colidir com o terreno) medida com dinâmica 6-DOF real do
  JSBSim, que um modelo cinemático 2D simplificado não pode honestamente alegar reproduzir.

**Catálogo** — as 225 classes nativas do MIXR (`base`/`models`/`simulation`/`terrain`/
`interop::dis`/`linkage`/`recorder`, o mesmo escopo de [`models/BUILT-IN.md`](../../models/BUILT-IN.md))
mais as 9 do plugin de produção `models/players/A-4` — só classe com despacho **real** num
`factory.cpp` (`new X()` alcançável), nunca "toda classe com `DECLARE_SUBCLASS` em algum header".
Por construção, toda entrada aqui já é "registrada em fábrica" — não existe mais um filtro/bloco
separado para classes declaradas-mas-não-despachadas. Busca por classe/fábrica/slot, filtros
(nome divergente, trabalha em fase, no cenário, decisão/UBF); clicar numa classe mostra o corpo
real de qualquer método que ela sobrescreve, quando conhecido.

**Estrutura** (o botão na página diz "Diagrama de Classes" — "Estrutura" aqui é só o nome
descritivo do conteúdo, mesma convenção do `CLAUDE.md`) — diagrama de classe UML sobre um recorte
curado de 19 classes fundacionais do MIXR
(`Referenced`/`Object`/`Component`/`Player`/`Station`/`Simulation`/`Agent`/`NetIO`/...): caixa
completa com atributos, componentes (membros de composição) e métodos, extraídos de verdade do
header C++ por `tools/extract_class_diagram.py` — não as 234 classes do Catálogo (225+9, ver acima), e não uma
segunda extração automática do mesmo escopo. Mais 28 alvos de composição em caixa mínima (Tier 2 —
só nome + base real, sem corpo). A TOPOLOGIA do diagrama (quem aparece filho de quem) e as notas de
"filosofia de emprego" no card de detalhe são organizadas **à mão** (`STRUCT_TOPOLOGY`/
`STRUCT_NOTES` em `doc.jsx`) — mesma ressalva da aba Comportamento: uma classe MIXR raramente tem
um único "pai" (ex.: `Player` é composto por `AbstractPlayer` via herança E é o molde de `Ntm` E é
apontado de volta por `System.ownship`), então a árvore escolhe UMA aresta primária por nó pra
caber num desenho legível; referências que fechariam ciclo aparecem tracejadas, sem diamante de
composição, sujeitas a envelhecer se a composição real mudar sem que a curadoria acompanhe. Nasce
totalmente expandido (recolher por caixa ou "recolher tudo"); clicar numa caixa cujo nome também
existe no Catálogo oferece "ver no Catálogo →" — ausente para `Referenced`/`Object`/`Component` e
outras que não têm entrada lá.

## Regenerar depois de editar `doc.jsx`

```bash
make open-docs   # regenera e ja abre
# ou, so para regenerar sem abrir navegador (ex.: ambiente headless):
python3 tools/generate_manual_catalog.py > docs/manual/catalog.generated.js && node docs/manual/compile.js
```

`index.html` **e** `catalog.generated.js` continuam committed — um clone que só quer LER a página
não precisa rodar nada antes; só editar o fonte (`doc.jsx`/o scanner Python) exige regenerar.

`CLASS_DIAGRAM` (a aba Estrutura) segue o mesmo precedente, já imperfeito, de `MODEL`/
`FLIGHT_MODEL`: não há passo de `make open-docs` que regenere sozinho — `python3
tools/extract_class_diagram.py` imprime o JSON (com auto-verificação embutida, sai com código≠0 se
falhar) pra colar manualmente como `const CLASS_DIAGRAM = {...};` em `doc.jsx`, precedido do
comentário `/* GERADO por tools/extract_class_diagram.py. Nao editar. */`. `STRUCT_TOPOLOGY`/
`STRUCT_BACKREFS`/`STRUCT_NOTES` (a curadoria) não são gerados por nada — são editados direto no
`doc.jsx`.

## Leia mais

[CLAUDE.md](../../CLAUDE.md), seção `docs/` — toda decisão de design e armadilha confirmada
rodando. O editor gráfico de cenário `.edl` (ferramenta de autoria, não visualização) mora em
[`src/ui/`](../../src/ui/README.md), não aqui.
