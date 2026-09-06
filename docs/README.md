# `docs/` — explorador de execução, EDL e classes built-in do MIXR

`index.html` é uma página estática, sem dependências de rede (React, ReactDOM e todo o app
ficam embutidos no próprio arquivo), com três visões sobre o framework:

1. **Execução** — o mesmo ciclo de fases já visto na aba "Componentes" (F6) do `./app`
   (dynamics/transmit/receive/process + as duas threads de decisão/fundo), aqui desenhado
   sobre a árvore de classes do MIXR com um grafo navegável (pan/zoom, arrastar) e timeline
   com transporte (tocar/pausar/velocidade/passo/reiniciar) e trilha "ociosos"/"ligações por
   nome". Cenário único, propositalmente exaustivo: um `( Aircraft )` carregando os DEZ
   sistemas primários que `Player::updateSystemPointers()` resolve por tipo (mesmo desenho de
   `src/poc/built-in_mixr_1` — ver o CLAUDE.md), ~72 nós, cobrindo praticamente tudo que dá
   para montar só com componentes nativos do `mixr::models` (a única troca deliberada: o
   `Datalink` aqui é nativo, não o `AlertDatalink` do plugin de voo). O botão "▾/▸ detalhe"
   no canto do grafo oculta o painel de baixo (Passo/Código/EDL/Classe) para dar mais área ao
   grafo; "▸ como ler um cartão", na legenda, abre um cartão de exemplo anotado explicando
   nome/subtítulo/pips de fase/contador de visitas/cor de thread. O botão "☾/☀" no canto
   superior direito alterna **claro/escuro** (persiste em `localStorage`, nunca segue
   `prefers-color-scheme` do SO — só o toggle decide); toda cor do CSS é uma variável
   (`--paper`/`--ink`/`--hot`/...), então o tema troca sem nenhum hex cru sobrando.

   O checkbox **"Árvore vertical"** experimenta uma segunda orientação do grafo (raiz em
   cima, irmãos lado a lado, em vez de raiz à esquerda) — `layout()` é uma função só, que
   troca qual eixo é "profundidade" e qual é "espalhamento dos irmãos"; o resto (W/H,
   margens, aresta em cotovelo, rótulo "via:"/"dt") tem uma versão própria por orientação.
   Para ESTE cenário (bem mais largo em folhas que fundo em profundidade) o resultado sai
   extremamente largo e raso — funciona (zero sobreposição, verificado nó a nó e rótulo a
   rótulo, inclusive nos fan-outs mais largos como os 6 filhos do Gimbal), mas horizontal
   continua sendo a orientação prática pra este cenário especificamente. "Ligações por nome"
   fica indisponível nessa orientação (as setas pontilhadas não têm posição calibrada nela
   ainda). Trocar de orientação reseta o pan/zoom (as posições dos nós mudam inteiras).

   O zoom é uma **barra deslizante** (0.4×–10×, ao lado de "▾ detalhe"/"ajustar" — a roda do
   mouse continua funcionando também) que reflete o zoom atual não importa a origem (arrasto,
   roda, "ajustar" ou o próprio "Seguir ramo"). O checkbox **"Seguir ramo"**, na barra de
   transporte, faz o pan acompanhar sozinho — com transição suave, inclusive passo a passo —
   o elemento (o "componente de atuação") ativo em cada passo, no MESMO zoom que já estava
   selecionado: o zoom é escolha do usuário e **persiste** entre passos (não é recalculado a
   cada evolução — uma versão anterior ajustava a caixa do caminho inteiro e o zoom "pulava" a
   cada passo); reajustar a barra enquanto "Seguir ramo" está ligado recentraliza o MESMO nó
   no zoom novo. Clicar num cartão do grafo **pausa a reprodução** e fixa o nó (📌, com um
   botão "soltar") num cartão próprio, acima das abas de detalhe — dado que não muda ao
   avançar/voltar o passo (quantos passos desta trilha visitam aquele nó), ao contrário do
   "×n" no próprio cartão, que só conta até o passo atual.

   O MESMO clique também abre um **popup flutuante** ancorado perto de onde o mouse caiu, com
   dado que a página nunca expôs fora do Catálogo: o **nome de fábrica** (`factoryOf()`, já
   extraído em `MODEL`), marcado se diverge do nome da classe C++ (ex.: `SimpleStoresMgr`
   registra como `"StoresMgr"` — a mesma armadilha do CLAUDE.md), se a classe está de fato
   **registrada em fábrica** e quantos **slots próprios** ela declara. Um botão "Ver classe
   completa no Catálogo →" salta pra lá com o cartão já aberto (`catalogFocus`, espelhando
   `focus` — o mesmo mecanismo que já levava Catálogo→Execução, agora nos dois sentidos). O
   popup fecha ao soltar o pino, trocar de nó/trilha/orientação, ou pelo próprio "×"; por ser
   `position:absolute` dentro de `.mx-graph` (que já corta overflow), a posição é clampada em
   JS pra nunca vazar da borda do grafo.

   **Bug real encontrado escrevendo isto, não só neste popup — o clique em QUALQUER nó do
   grafo, com um mouse de verdade.** `onDown` (pan/arrastar) chamava `setPointerCapture()` no
   `<svg>` assim que o dedo/botão descia, incondicionalmente. Um clique de verdade
   (mousedown+mouseup no MESMO lugar, sem arrastar nada) ainda assim capturava o pointer — e o
   browser retargeta o `click` resultante pro elemento que capturou (`<svg>`), nunca chegando
   no `<g class="mx-node">` por baixo. Confirmado com um listener de depuração: `pointerdown`
   mostrava `target=rect`, mas `pointerup`/`click` mostravam `target=svg`; soltar a captura no
   `pointerup` (tentativa óbvia) NÃO resolvia — a decisão de retargetar o `click` já estava
   tomada no momento da captura, não no da liberação. É por isso que testes anteriores desta
   página "confirmavam" o clique-pra-fixar funcionando: usavam `dispatchEvent` sintético, que
   não passa por pointer capture nenhum — nunca exercitando o caminho real de mouse. Corrigido
   adiando a captura pro primeiro `pointermove` que de fato deslocar além de 4px
   (`DRAG_CLICK_PX`): um clique sem deslocamento nunca chega a capturar o pointer, então o
   `click` segue o alvo normal; um arrasto de verdade continua capturando (e panando) exatamente
   como antes, só um evento mais tarde.

   Clicar num dos **quatro quadradinhos de fase** dentro do cartão (não o cartão inteiro) pula
   a reprodução **direto pro passo em que aquele nó roda naquela fase** — sem procurar
   manualmente na timeline. `e.stopPropagation()` no clique do pip evita que o mesmo clique
   TAMBÉM fixe/abra o popup do cartão (os dois clicáveis convivem sem disputa: pip pula o
   passo, o resto do cartão fixa). Só quadradinhos **preenchidos** (o nó de fato participa
   daquela fase — `has`, o mesmo dado que já colore o pip) são clicáveis; um quadradinho vazio
   deixa o clique atravessar pro cartão de baixo, como sempre foi. A busca (`performPhaseJump`)
   começa no passo SEGUINTE ao atual e dá a volta — clicar de novo no MESMO pip avança pro
   próximo quadro em vez de ficar preso no primeiro achado, útil pra comparar frames diferentes.
   Como fase só existe de verdade na trilha "Thread de Tempo Crítico", clicar um pip com
   "Thread de fundo"/"Reset" selecionada troca pra ela primeiro e resolve o salto assim que o
   trace novo estiver pronto (`phaseJumpRequest` + um efeito que dispara só depois do `trace`
   já refletir a trilha nova — sem isso o salto rodaria contra o trace ANTIGO, ainda em memória
   por um render). Testado nos dois temas e nas duas orientações, e com "Seguir ramo" ligado
   (o pan de acompanhamento já reage a qualquer mudança de `idx`, então o salto por fase herda
   esse comportamento de graça).

   A trilha "Quadro" chama-se **"Thread de Tempo Crítico"**: o ciclo de decisão do
   `mixr::base::ubf` (`AgentTC`/`Arbiter`/`AbstractState`/`AbstractBehavior`/`AbstractAction`
   — percepção → cada behavior vota → `Arbiter::genComplexAction()` escolhe o de maior voto →
   a ação, efêmera, atua no ator) roda no MESMO pool de tempo crítico que as 4 fases — é
   `AgentTC`, não `Agent`, a mesma escolha da produção deste repositório (`FlightAgentTC`) por
   determinismo — então ele aparece **dentro** desta trilha (na fase 0 de cada quadro, dentro
   de `walk()`), não numa trilha à parte. Sem filtro de fase, `controller()` roda de novo nas
   fases 1-3 (mesma decisão, repetida) — resumido por padrão, visível com "Ociosos". O nó
   `AgentTC` mora DENTRO do `( Aircraft )` (resolve o ator por containment, já que `AgentTC`
   não sobrescreve `initActor()`), não como irmão de `simulation:` ligado por nome. Escopo
   deliberado: **só o framework**, nada deste repositório — os nós/slots/código
   (`AgentTC::updateTC`, `Agent::controller`, `Arbiter::genComplexAction`, ...) são reais e
   extraídos como o resto da página, mas os "behaviors" e os votos (10/6/3) são didáticos, e o
   EDL mostrado nessa subárvore é ilustrativo (rotulado como tal na própria aba EDL —
   inclusive o aviso de que `UbfAgentTC` não está de fato encadeada em `base/factory.cpp`),
   não o `agent1:` de produção — que declara `BtBehavior`/`FlightState`/
   `AltitudeSafetyBehavior`, classes do plugin de voo (`models/player/A4/`), fora do que esta
   página descreve. Ver a nota no subtree de `SCENARIO` (dentro do `( Aircraft )`) e o
   comentário no bloco `if (node.id === "agent")` de `walk()`, em `doc.jsx`.

   **Bug real encontrado — a faixa de destaque escondia o cabeçalho de coluna.** A banda
   `fill="var(--band-bg)"` que ilumina toda a subárvore de `components:` de cada `( Aircraft )`
   era desenhada DEPOIS dos rótulos de coluna (`DEPTH_LABELS` — "subsistemas"/"detalhe"/
   "ações"), e em SVG quem desenha por último fica POR CIMA: as três etiquetas ficavam
   100% cobertas pela própria banda (sobreposição vertical completa, medida via `getBBox()`,
   não só "perto" — o texto simplesmente não aparecia, em nenhum tema). Corrigido invertendo a
   ordem de dois blocos JSX (banda antes, rótulos depois) — nenhuma mudança de geometria, só de
   ordem de pintura. Confirmado nos dois temas e nas duas orientações.

2. **Eventos** — aba dedicada só a ilustrar os quatro momentos "kind" especiais que já existem
   dentro de "Thread de Tempo Crítico"/"Reset" (`rf`/`name`/`release`/`vanish`) — fáceis de
   perder no meio de ~258 passos de visita rotineira. Não reintroduz o grafo pan/zoom
   completo: cada evento é uma ponte entre DOIS pontos da árvore sem relação de pai-filho, e o
   que importa mostrar é de onde cada lado desce (a cadeia raiz→nó, um "breadcrumb" HTML) e o
   que liga os dois — texto real, extraído como o resto da página, filtrado das MESMAS
   `traceFrames()`/`traceReset()` (`eventTour()`, em vez de duplicar o texto: editar uma
   descrição de evento em `walk()`/`traceReset()` atualiza esta aba de graça). Quatro cartões
   clicáveis (RF/Nome/Liberação/Reset), setas do teclado, e uma nota fixa por evento explicando
   POR QUE ele escapa de qualquer leitura estática:
   - **RF** — `Radar::transmit()` → `Antenna::rfTransmit()` → `alvo->event(RF_EMISSION)`: o
     sensor aponta pro alvo por PARÂMETRO de runtime, não por vizinhança na árvore.
   - **Nome** — `Tws`/`Stt` apontam pro MESMO `AirTrkMgr` por uma STRING (`trackManagerName`)
     resolvida em runtime; um erro de digitação não dá erro de carga, só um sistema mudo.
   - **Liberação** — `Stores::releaseWeapon()` insere um player novo (`addNewPlayer()`) que
     nenhum arquivo de configuração descreve; a árvore de contenção muda em execução.
   - **Reset** — o inverso da liberação: `processComponents()` reconstrói a partir de
     `origPlayers`, que NUNCA teve o míssil — ele some sem uma linha de código que o remova.

   A direção de cada ponte é "quem SEGURA o ponteiro → quem é achado por ele", não sempre
   `node`→`to` da trilha original: no evento de Nome, o push acontece no turno de visita do
   PRÓPRIO track manager (`node: "ttm"`), mas quem guarda o slot é o sensor (`from: "tws"`) —
   mostrar `ttm`→`tws` inverteria o sentido real da referência.

3. **Catálogo** — as 342 classes que `DECLARE_SUBCLASS` no fork, cruzadas com quem de fato
   se registra em fábrica (`IMPLEMENT_*SUBCLASS`, 224 delas — 48 com nome de fábrica
   divergente do nome da classe), quem tem slot (`BEGIN_SLOTTABLE`/`END_SLOTTABLE`, 644
   slots em 135 classes) e quem participa do despacho por fase. Busca por classe, nome de
   fábrica ou slot; filtros por "nome divergente", "trabalha em fase", "não registradas",
   "no cenário" (as classes que os cenários deste repositório de fato instanciam) e "Decisão
   (UBF)" (cadeia de herança toca `Agent`/`AbstractBehavior`/`AbstractState`/`AbstractAction`
   — pega também `mixr::models::Action`, que de fato deriva de `AbstractAction`, cobrindo os
   quatro `Action*` de steerpoint do cenário). Clicar numa classe abre o cartão e, se algum
   método que ELA MESMA sobrescreve (não herdado) estiver em `SNIPPETS`, mostra o corpo real
   logo abaixo dos slots — a busca é pelas chaves de `SNIPPETS` por prefixo `Classe::`, não
   pela lista de overrides que `MODEL` guardou (essa fica defasada quando `TARGET_METHODS` do
   script ganha um método novo sem o `MODEL` embutido ser regenerado — foi exatamente o caso
   de `Agent::controller`, capturado em `SNIPPETS` mas ausente do `ov` gravado). Sem nenhum
   método curado para aquela classe (a maioria das 342 — `SNIPPETS` é uma curadoria pequena,
   não o fonte inteiro), o cartão diz isso explicitamente em vez de fingir que não há nada.

4. **AgentTC→UBF→BT** — um ENSAIO, não uma quarta extração. As três abas acima mostram o
   framework MIXR genérico (a aba Execução chega a modelar um `AgentTC`/`UbfArbiter` didático,
   mas com "behaviors" e votos inventados — ver a nota no bloco `if (node.id === "agent")` de
   `walk()`). Esta aba troca isso pela cadeia de decisão REAL deste repositório — o modelo de
   voo em `models/player/A4/` que `single-thread`/`multi-thread`/`app` de fato rodam a 50 Hz, por
   aeronave, na fase 3 do frame de tempo crítico:
   `FlightAgentTC → Agent::controller → UbfArbiter → {AltitudeSafetyBehavior, BtBehavior} →
   flight_tree.xml (Fallback) → FlightAction::execute() → Autopilot/xboard/LOG`.

   Cinco cenários (Patrulha/Combustível baixo/Contato detectado/Alerta recebido/Piso de
   segurança) — cada um fecha uma condição diferente do `Fallback` de produção (ou, no último,
   nenhuma: a árvore recomenda `PATROL`, mas perde a votação para `AltitudeSafetyBehavior`,
   voto 90 contra 50). Escolher um cenário reconstrói a trilha de passos inteira; "reproduzir"
   avança sozinho (1500 ms/passo, mesmo padrão de `setTimeout` da aba Execução), `←`/`→`
   também navegam.

   **Segunda passada, motivada por feedback ("elabore mais em prol da clareza"), com dois
   reforços — um de conteúdo, um visual:**

   - **Um glossário recolhível** ("▸ o que são UBF e BT?", mesmo padrão de disclosure de
     "▸ como ler um cartão" na aba Execução) — duas colunas lado a lado: os 4 papéis do UBF
     (`State`/`Behavior`/`Arbiter`/`Action`, cada um amarrado à classe real deste modelo que o
     preenche) e os 4 blocos do BT que aparecem em `flight_tree.xml` (`Fallback "?"`/
     `Sequence "→"`/`Condition`/`Action`), fechando com a frase-chave: "`flight_tree.xml`
     inteira é só a política de UM `Behavior` (`BtBehavior`) dentro do `UbfArbiter` — o UBF não
     sabe que existe uma árvore ali dentro".
   - **Cada ramo do `Fallback` deixou de ser UMA caixa e virou a `Sequence` que ele de fato É**
     — `Condition` e `Action` como dois nós PRÓPRIOS, com estado próprio, dentro de uma moldura
     tracejada rotulada com o nome da `Sequence` (`rtb_sequence`/`engage_sequence`/
     `support_sequence`; `Patrol` continua sozinho, por não ter `Condition` — é o "senão" da
     árvore). Uma `Sequence` vencedora agora anima em DOIS passos (a `Condition` sucede,
     DEPOIS a `Action` sucede) em vez de um só — é a semântica de "→" (E lógico, para no
     primeiro `FAILURE`) se revelando na prática, não só descrita em texto. Isso também separa
     dois curto-circuitos que a primeira versão confundia num símbolo só: `✕` (uma `Condition`
     foi perguntada e falhou — curto-circuito DENTRO da `Sequence`, e a `Action` irmã dela vira
     `⋯`) contra `⋯` de um ramo inteiro (nem a própria `Condition` chegou a ser perguntada —
     curto-circuito do `Fallback`, um nível acima).
   - **Toda caixa do diagrama ganhou borda esquerda colorida por FRAMEWORK** — azul
     (`var(--bgc)`) para um papel do UBF, roxa (`var(--new)`) para um nó do BT — então a
     pergunta "isso que estou olhando é UBF ou é BT?" tem resposta visual imediata, sem
     depender de ler o texto. A transição de cor acontece exatamente na caixa `BtBehavior`
     (ainda UBF, azul) → `tree.tickRoot()` (já BT, roxa): o ponto exato onde um Behavior
     delega sua decisão a uma árvore.

**Curada, não instrumentada — com UMA exceção deliberada.** As três primeiras abas: sem
processo MIXR rodando por trás, herança, nome de fábrica, registro, slots, fases e os trechos de
código (com arquivo e linha reais) foram extraídos direto da árvore de fontes
(`contexts/src/mixr/`, fork v170600) pelo script `scripts/extract_execution_chain.py` (fora deste
diretório) e embutidos em `docs/doc.jsx` como os objetos `MODEL`/`FACTORIES`/`SNIPPETS`/`STATS` —
nada ali é digitado à mão. Isso está avisado na própria página e não deve ser removido em
incrementos futuros. A árvore do cenário (`SCENARIO`/`ALL`/`EDL_TEXT`) é composição manual sobre
esses mesmos dados — cada classe usada já vinha do `MODEL` gerado, só a escolha de QUAIS classes
e em que arranjo é curada.

A aba 4 é a exceção: não há script de extração para `models/player/A4/` (é código deste
repositório, não do fork vendorizado), então `UBFBT_SCENARIOS`/`UBFBT_BRANCHES`/
`buildUbfBtSteps()` em `docs/doc.jsx` foram escritos à mão, lendo `FlightAgentTC.{hpp,cpp}`,
`Agent.cpp`/`Arbiter.cpp` (framework), `BtBehavior.cpp`, `AltitudeSafetyBehavior.cpp`,
`flight_tree.xml`, `bt/nodes/*.cpp` e `FlightAction.cpp` linha a linha — cada passo cita
arquivo:linha, mas nenhuma passada automática vigia se o texto ainda bate com o código. Se
`BtBehavior`/a árvore/`FlightAction` mudarem, esta aba pode ficar desatualizada EM SILÊNCIO,
diferente das outras três. Rótulo de "ensaio" no próprio texto da aba é proposital — é o convite
para promovê-la a extração de verdade (ou pelo menos revisá-la) se o modelo mudar.

## Como abrir

Direto no navegador, sem servidor nenhum (zero requisições de rede, inclusive React):

```
xdg-open docs/index.html     # ou file://.../docs/index.html
```

Ou servido (para simular hospedagem futura, ex. GitHub Pages):

```
python3 -m http.server --directory . 8000
# abrir http://localhost:8000/docs/
```

## `doc.jsx` → `index.html`

`doc.jsx` é a fonte (um componente React único, `App`, com CSS embutido em uma string e os
dados gerados embutidos como constantes — nenhum `fetch`/`import` além de `react`).
`index.html` é gerado a partir dele: JSX transpilado para `React.createElement` (Babel,
preset `react`, via `docs/compile.js`) e React 18 + ReactDOM 18 (UMD, produção) inlinados no
mesmo arquivo, para que abrir `index.html` não dispare nenhuma requisição de rede. Regenerar
depois de editar `doc.jsx`:

```bash
make docs        # ou: node docs/compile.js
```

A primeira execução baixa React/ReactDOM 18.3.1 UMD e instala `@babel/standalone` em
`docs/.cache/` (gitignored) — as próximas rodam sem rede nenhuma, direto do cache. `make
open-docs` continua existindo à parte: só ABRE o `index.html` já gerado, nunca regenera.

## O editor gráfico de cenário .edl mora em `src/ui/`, não aqui

`src/ui/edl_builder.jsx` (+ `edl-builder.html` gerado) é uma página irmã no MESMO padrão sem-
bundler deste diretório (React + Babel via um `compile.js` próprio, `src/ui/compile.js`) — mas
vive fora de `docs/` de propósito: é uma ferramenta de verdade (criar um cenário `.edl` do zero
arrastando classes de uma paleta), não documentação/visualização do framework. Ver
`src/ui/README.md`.

**`scripts/generate_catalog_js.py` foi removido**: gerava `CATALOG_SRC`/`TAXONOMY`/
`CATALOG_TOUR` a partir de `--catalog`, mas nenhuma das três constantes chegou a ser consumida
por `doc.jsx` — confirmado, `grep` não achava nenhuma na página. O papel que ele cumpriria
(catálogo completo de classes) está coberto, e mais amplamente, pelo modo `--edl-catalog` de
`scripts/extract_execution_chain.py` que alimenta o editor gráfico acima.
