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

**Curada, não instrumentada.** Sem processo MIXR rodando por trás — herança, nome de
fábrica, registro, slots, fases e os trechos de código (com arquivo e linha reais) foram
extraídos direto da árvore de fontes (`contexts/src/mixr/`, fork v170600) pelo script
`scripts/extract_execution_chain.py` (fora deste diretório) e embutidos em `docs/doc.jsx` como os
objetos `MODEL`/`FACTORIES`/`SNIPPETS`/`STATS` — nada ali é digitado à mão. Isso está avisado na
própria página e não deve ser removido em incrementos futuros. A árvore do cenário
(`SCENARIO`/`ALL`/`EDL_TEXT`) é composição manual sobre esses mesmos dados — cada classe usada
já vinha do `MODEL` gerado, só a escolha de QUAIS classes e em que arranjo é curada.

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

## O que fica para os próximos incrementos

- `scripts/generate_catalog_js.py` (versionado) gera `CATALOG_SRC`/`TAXONOMY`/`CATALOG_TOUR` a
  partir de `extract_execution_chain.py --catalog`, mas **nenhuma das três constantes é usada por
  `doc.jsx`** hoje — confirmado (`grep` não acha nenhuma delas na página). Parece um caminho
  alternativo de geração, de uma época em que a página ainda tinha um segundo modo ("Catálogo
  completo", removido — o cenário único de hoje já cobre a maior parte das classes built-in);
  vale decidir entre reaproveitar ou remover o script.
