## Contexto

Este repositório documenta o **MIXR** (framework C++ de modelagem e simulação) e o seu
emprego. A documentação em prosa já existe e é boa, mas há uma classe de conhecimento que
ela não consegue transmitir: **a ordem em que as coisas se chamam durante a execução**, e
o fato de que boa parte dos componentes built-in declarados num cenário nunca executa nada
por fase.

Sua tarefa é construir um **explorador de execução**: uma aplicação web estática,
navegável e animada, que percorre um cenário MIXR chamada por chamada, exibindo o
código-fonte real que está executando a cada passo.

Antes de escrever qualquer código, leia:

- `<docs/MIXR-CONTEXT.md>` — funcionamento interno (classes, macros, ciclo de vida, EDL)
- `<docs/MIXR-PATTERN-CONTEXT.md>` — padrões de emprego observados em `examples/`
- `<scripts/check-edl.py>` — o extrator já existente de fábricas, herança e slots
- `<poc/*/>` — cenários `.edl` reais deste repositório

Seções especialmente relevantes de `MIXR-CONTEXT.md`: §5.1 (árvore de contenção), §5.3
(propagação do tempo e `tcFrame`), §21.1 (ciclo/frame/fase), §21.5 (`Simulation`), §21.6
(`Station`), §23.2 (`Player`), §23.5 (cadeia RF), §23.9 (`StoresMgr`), §29.5 (roteiro do
quadro), §30 (catálogo de armadilhas).

---

## Regra de ouro

**Todo trecho de C++ exibido pela aplicação é transcrição, nunca paráfrase.** Cada trecho
sai da árvore de fontes ou dos blocos já conferidos nos documentos de contexto, e carrega
o caminho do arquivo de origem. Se você não encontrar o fonte de um método, **não invente
o corpo** — omita o passo ou marque-o explicitamente como condensado.

O mesmo vale para nomes de classe, nomes de fábrica, nomes de slot e valores padrão:
todos precisam bater com o fonte. Use `check-edl.py` como oráculo — se ele não reconhece
um nome que você usou no cenário, o nome está errado.

---

## Entregável

```
<docs/explorador/>
  index.html          # ponto de entrada, sem build obrigatório
  src/
    App.jsx           # a UI (não conhece MIXR)
    data/scenario.js  # a árvore de contenção
    data/sources.js   # os trechos de fonte, indexados por método
    data/traces.js    # os geradores de passos
  README.md           # como rodar, como estender, como plugar um trace real
```

Prefira Vite + React. Se a política do repositório for evitar toolchain JS, entregue um
único `.html` autossuficiente com React via CDN — mas mantenha a separação lógica dos
quatro arquivos acima dentro dele.

---

## Arquitetura obrigatória

A aplicação é um **reprodutor de trace**, não uma animação. A UI não pode conter nenhuma
regra do MIXR. Três estruturas de dados a alimentam:

### 1. `SCENARIO` — a árvore de contenção

Espelha o que um `.edl` constrói. Cada nó:

```js
{
  id: "radar",                  // único
  cls: "Radar",                 // nome de CLASSE (não de fábrica)
  factory: "Radar",             // nome de FÁBRICA, quando divergir da classe
  player: false,                // é um Player (entra na lista da Simulation)?
  phases: [1, 2, 3],            // fases em que este nó faz trabalho; [] = ocioso
  chain: [                      // cadeia BaseClass::, do concreto ao Component
    ["Radar",     "transmit / receive / process"],
    ["RfSensor",  "gerencia a varredura e a fila de relatórios"],
    ["RfSystem",  "updateData() monta o Tdb — na thread de fundo"],
    ["System",    "dt4 = dt*4; switch(phase)"],
    ["Component", "percorre a antena"]
  ],
  note: "armadilha ou observação exibida ao inspecionar o nó",
  dynamic: false,               // criado em execução (arma lançada)
  children: [...]
}
```

Inclua **built-ins ociosos de propósito** — `Antenna`, `Navigation`, `Route`,
`SigSphere`, `QuadMap`, `Ins`, `Gps`. O maior valor pedagógico da ferramenta é mostrar
esses nós permanecendo apagados nas quatro fases, com a explicação ao lado (`Gimbal` não
sobrescreve `updateTC()`; `Ins` e `Gps` não executam nada).

### 2. `SRC` — os trechos de fonte

```js
{ "System::updateTC": { file: "src/models/system/System.cpp", lines: [ ... ] } }
```

Mínimo a incluir: `Station::updateTC`, `Station::updateData`, `Simulation::updateTC` (o
laço de quatro fases com a barreira de `SyncThread`), `Component::updateTC`,
`Component::tcFrame`, `Component::processComponents`, `Player::updateTC`,
`Player::updateSystemPointers`, `System::updateTC`, `Autopilot::process`,
`Gimbal::processPlayersOfInterest`, `Stores::releaseWeapon`, a declaração das duas listas
de players de `Simulation.hpp`, e a cadeia RF condensada de §23.5.

### 3. `TRACES` — geradores de passos

Cada gerador devolve uma lista ordenada de passos:

```js
{
  kind: "visit" | "phase" | "rf" | "release" | "reset" | "vanish" | "note",
  node: "radar",                // nó destacado na árvore
  to: "mig",                    // destino, para arestas de evento
  counters: { cycle, frame, phase, exec, simT },
  dt: 0.005,                    // dt recebido NESTE nível
  src: "System::updateTC",      // chave em SRC
  hl: [4, 9],                   // intervalo de linhas destacado
  stack: [{ label, node }, ...],// pilha de chamadas no momento
  runs: true,                   // executou trabalho de fase?
  idle: false,                  // visitado sem trabalho (filtrável)
  title, body,                  // texto do painel
  warn                          // armadilha, destacada em vermelho
}
```

Implemente **três cadeias**, selecionáveis por abas:

**a) Quadro (tempo crítico).** Três quadros consecutivos. Por quadro: os sete passos de
`Station::updateTC`, depois quatro fases, e em cada fase a travessia completa da lista de
players com `dt/4`. Nos nós que despacham por fase, o passo deve mostrar explicitamente
`dt` recebido e `dt4 = dt*4`, e o texto deve dizer que a divisão na descida e a
multiplicação na chegada se cancelam — o método de fase roda **uma vez por quadro com o
dt integral**. Eventos a injetar:

- fase 1: `Antenna::rfTransmit()` → `alvo->event(RF_EMISSION)`, desenhado como aresta
  tracejada entre objetos, com o aviso de que o `Tdb` consultado veio da thread de fundo;
- fase 3: `Radar::process()` → `TrackManager::newReport()`, aresta rotulada como
  resolvida por string (`trackManagerName`), com o aviso de que um erro de digitação ali
  não gera erro de carga;
- fase 3 do segundo quadro: `Stores::releaseWeapon()` — o `Missile` sai do cabide, entra
  na lista ativa via `addNewPlayer()` e passa a ser percorrido nas quatro fases do quadro
  seguinte. A árvore muda em execução.

**b) Thread de fundo.** `Station::updateData()` com os três ramos condicionais,
`Navigation::updateData()` lendo a `Route`, `Gimbal::processPlayersOfInterest()` montando
o `Tdb`, e a publicação do Entity State PDU pela thread de rede. O ponto a transmitir: a
taxa é própria (`bgRate`), não há `dataFrame()`, e portanto este caminho não é
instrumentado.

**c) Reset.** `RESET_EVENT` descendo a árvore; `players` reconstruída a partir de
`origPlayers`; e o **desaparecimento do míssil** que a cadeia (a) criou — sem que exista
nenhuma linha de código para removê-lo. Esta é a carga útil da cadeia: só funciona porque
o usuário viu a arma nascer na aba anterior.

---

## Requisitos de interface

Três colunas: árvore SVG à esquerda, código ao centro, painéis à direita.

- **Árvore.** Nó em execução destacado; nós na pilha de chamadas com contorno intermediário;
  nós ainda não criados tracejados e translúcidos. Arestas de evento e de resolução por
  nome aparecem apenas no passo em que ocorrem, com legenda.
- **Código.** Fonte com numeração, faixa destacada e rolagem automática até ela. Caminho do
  arquivo acima do painel.
- **Painéis.** Passo atual (título, explicação, armadilha em destaque), pilha de chamadas,
  cadeia de herança do nó corrente com o nível que faz `switch(phase)` marcado.
- **Contadores.** `cycle()`, `frame()`, `phase()`, `execCounter`, tempo simulado, `dt`
  recebido. Faixa das quatro fases com a fase corrente acesa.
- **Transporte.** Reproduzir/pausar, passo a frente e a trás, barra de rolagem sobre o
  trace, controle de velocidade, atalhos ← → e espaço.
- **Filtro "nós ociosos"**, desligado por padrão: esconde as visitas em que o nó não faz
  trabalho, reduzindo o trace a um tamanho legível.
- Clicar num nó fixa a inspeção nele.

Acessibilidade e disciplina: foco de teclado visível, `prefers-reduced-motion` respeitado,
responsivo até uma coluna, sem `localStorage`, sem `<form>`.

---

## Fases de trabalho

Pare ao fim de cada fase e me mostre o resultado antes de seguir.

1. **Levantamento.** Extraia do fonte, com um script reproduzível em `scripts/`, a lista
   de classes que sobrescrevem `updateTC`, `updateData`, `dynamics`, `transmit`,
   `receive`, `process` e `processComponents`, e a cadeia `DECLARE_SUBCLASS` de cada uma.
   Reaproveite o que `check-edl.py` já faz. Entregue a tabela; **não escreva UI ainda**.
2. **Dados.** Escreva `scenario.js` e `sources.js` a partir dessa tabela. Valide os nomes
   contra o extrator. Entregue o cenário e o inventário de trechos.
3. **Traces.** Escreva os três geradores. Entregue a contagem de passos por cadeia e um
   despejo textual do trace do quadro 0, para conferência da ordem.
4. **UI.** Só agora. Construa a interface sobre os dados já validados.
5. **README.** Como rodar, como acrescentar um nó, como acrescentar um trecho de fonte, e
   o contrato do formato de trace.

---

## Não faça

- Não invente corpos de método, nomes de slot, valores padrão nem nomes de fábrica.
- Não coloque lógica do MIXR na UI: se a UI precisa saber o que é uma fase para desenhar,
  o dado está modelado errado.
- Não use biblioteca de grafo com layout automático. A árvore tem profundidade fixa e
  cabe num layout recursivo de trinta linhas; força dirigida só embaralha.
- Não use animação contínua por `requestAnimationFrame`. O tempo é um índice de array:
  determinístico, endereçável, com rolagem funcionando de graça.
- Não suavize as armadilhas. Elas são o motivo de a ferramenta existir.

---

## Critério de aceite

1. Percorrer a cadeia (a) inteira sem que nenhum nó exiba código que não esteja no fonte.
2. `Antenna`, `Navigation`, `Route` e as assinaturas permanecem apagadas nas quatro fases,
   com a explicação disponível ao clicar.
3. Em qualquer nó que despache por fase, o painel mostra os dois valores de `dt` e o
   painel de herança marca o nível exato onde o `switch` acontece.
4. O míssil lançado na cadeia (a) aparece na árvore e some na cadeia (c).
5. Nenhum `console.error` no carregamento; a página funciona sem servidor de aplicação.

---

## Extensão prevista (não implemente agora, mas não impeça)

O passo seguinte é instrumentar `Component::tcFrame()` no fork para emitir
`(execCounter, phase, ponteiro, caminho na árvore, dt, entrada/saída)` num buffer
circular, despejado no encerramento em Chrome Trace Format. Um leitor desse JSON
substituirá `traces.js` sem tocar na UI — mesma interface, execução real em vez de trace
sintético. Mantenha a fronteira entre dados e UI nítida o bastante para que essa troca
seja de um arquivo só, e documente o contrato no README.