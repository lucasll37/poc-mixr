# `docs/manual/` — explorador de execução, EDL e classes built-in do MIXR

Página estática (React embutido, zero rede para abrir) com cinco visões sobre o framework,
**curadas, não instrumentadas** — sem processo MIXR rodando por trás, herança, nome de fábrica,
registro, slots, fases e os trechos de código (com arquivo e linha reais) vêm de
`tools/generate_manual_catalog.py` (que reaproveita `tools/mixr_source_scan.py` e
`tools/extract_execution_chain.py` como módulos) escaneando o fonte de verdade
(`contexts/src/mixr/` e `models/players/air/A-4/`) — nada digitado à mão.

Todo bloco de código C++ das cinco abas (Execução, Comportamento, step-by-step, Estrutura, Catálogo) passa
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
conteúdo) — o ciclo de fases do MIXR (dynamics/transmit/receive/
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
não extração automática — só que atravessando dois plugins (`models/players/air/A-4` +
`models/players/weapon/missile`), classes abstratas do MIXR (`AbstractWeapon`, `StoresMgr`) que o
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

**Catálogo** — as 225 classes nativas do MIXR (`base`/`models`/`simulation`/`terrain`/
`interop::dis`/`linkage`/`recorder`, o mesmo escopo de [`models/BUILT-IN.md`](../../models/BUILT-IN.md))
mais as 9 do plugin de produção `models/players/air/A-4` — só classe com despacho **real** num
`factory.cpp` (`new X()` alcançável), nunca "toda classe com `DECLARE_SUBCLASS` em algum header".
Por construção, toda entrada aqui já é "registrada em fábrica" — não existe mais um filtro/bloco
separado para classes declaradas-mas-não-despachadas. Busca por classe/fábrica/slot, filtros
(nome divergente, trabalha em fase, no cenário, decisão/UBF); clicar numa classe mostra o corpo
real de qualquer método que ela sobrescreve, quando conhecido.

**Estrutura** (o botão na página diz "Diagrama de Classes" — "Estrutura" aqui é só o nome
descritivo do conteúdo) — diagrama de classe UML sobre um recorte
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

O editor gráfico de cenário `.edl` (ferramenta de autoria, não visualização) mora em
[`src/ui/`](../../src/ui/README.md), não aqui.
