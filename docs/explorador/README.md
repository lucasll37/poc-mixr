# Explorador de Execução MIXR

Reprodutor de trace (não uma animação) do frame de tempo crítico do MIXR: mostra, passo a
passo, a ordem real de chamadas — `Station` → `Simulation` → 4 fases → cada `Player` → cada
`System` — lado a lado com o trecho de código-fonte real que está executando, sobre a árvore
de contenção **real** do cenário `src/poc/built-in_mixr_1/configs/scenario_max_player.epp.in`
("o player máximo": 53 das 96 classes de `mixr::models`).

Implementa `docs/TODO.md`, mas **não ao pé da letra** — a seção "O que mudou em relação ao
TODO.md original", mais abaixo, documenta as correções feitas contra o fonte de verdade.

## Como rodar

Abra `index.html` direto no navegador (duplo clique, ou `file://.../docs/explorador/index.html`).
Não precisa de servidor, de build, nem de `npm install`. As duas únicas dependências externas
são React e ReactDOM via CDN (`cdnjs.cloudflare.com`) — o resto é um arquivo HTML autossuficiente
com JavaScript puro (`React.createElement`, sem JSX/Babel). Isso exige internet na primeira
carga (para buscar os dois `<script src>`); nada mais é buscado pela rede.

## Estrutura (um arquivo, quatro seções lógicas)

`index.html` tem quatro blocos `<script>`, na ordem que o `docs/TODO.md` pedia como arquivos
separados:

| bloco | papel |
|---|---|
| `SRC` (+ `flattenSrc`) | os trechos de fonte, indexados por chave `"Classe::metodo"` |
| `SCENARIO` | a árvore de contenção — nós, cadeia de herança, fases, notas |
| `TRACES` (+ `buildFrameTrace`/`buildBackgroundTrace`/`buildResetTrace`) | os geradores de passos das 3 cadeias |
| `App` | a UI React — não conhece nenhuma regra do MIXR, só consome os três dados acima |

## O contrato de cada dado

### `SRC[chave]`

```js
{
  file: 'contexts/src/mixr/src/...',
  blocks: [
    { start: 258, lines: [ 'linha 258 literal', 'linha 259 literal', ... ] },
    { omitted: 'descrição curta do que foi pulado' },
    { start: 300, lines: [ ... ] },
  ],
}
```

**Regra de ouro, e como o formato a impõe**: um `block` com `start`/`lines` é sempre uma cópia
**contígua e literal** do arquivo real — inclusive linhas em branco, para a numeração bater com
o arquivo (nunca duas linhas do fonte são fundidas numa só; blocos podem pular trechos, mas
nunca reescrevem o que mostram). Um `{ omitted: "..." }` marca explicitamente um trecho que
existe no arquivo mas não está aqui — a UI o renderiza como uma linha sem número, com `···`.
`flattenSrc(entry)` expande os blocos numa lista `{ln, text}` pronta para exibir; é a função que
os passos de trace (`hl: [inicio, fim]`) endereçam por número de linha REAL, não por índice
relativo — confira com `flattenSrc(SRC['Simulation::updateTC'])` no console se tiver dúvida
sobre algum número.

**Para acrescentar um trecho**: ache o método no fonte de verdade (`contexts/src/mixr/` para o
framework vendorizado, `models/` para os plugins deste repositório — note que `models/` estava
em reorganização quando isto foi escrito; os caminhos citados podem ter mudado, confira com
`find models -iname NomeDoArquivo.cpp`), copie linha a linha com o número real de início, e use
`scripts/extract_execution_chain.py` para confirmar que a classe/método realmente existe e
qual é a cadeia de herança antes de referenciá-lo num nó do `SCENARIO`.

### `SCENARIO`

Uma árvore de nós (função `n(id, cls, opts)`), espelhando `scenario_max_player.epp.in`:

```js
{
  id: 'radar',                 // único em toda a árvore
  cls: 'Antenna',               // nome de CLASSE C++
  factory: 'Antenna',           // nome de FÁBRICA (só difere em casos como StoresMgr/SimpleStoresMgr)
  player: false,
  phases: [0, 3],               // fases em que ALGUM nível da cadeia faz trabalho NÃO-VAZIO
                                 // (ignorando Object/Component/System, que só despacham);
                                 // 'bg' = updateData (fundo); [] = ocioso confirmado
  idle: false,                  // == phases.length===0, a menos que a nota explique algo mais fino
  chain: ['Antenna', 'ScanGimbal', 'Gimbal', 'System'],  // do concreto ao genérico
  dispatch: 'System',           // em que nível da cadeia mora o switch(phase)
  srcKeys: ['Antenna::rfTransmit'],
  dynamic: false,                // true = só passa a existir em execução (ver TRACES: spawn/despawn)
  note: '...',                   // a observação que dá o valor pedagógico ao nó
  children: [ ... ],
}
```

**Para acrescentar um nó**: rode `python3 scripts/extract_execution_chain.py NomeDaClasse` (ou
o nome de fábrica) para obter `chain`/`phases`/`idle` de verdade — não adivinhe. O script já
imprime, por nível da cadeia, qual dos 7 métodos (`updateTC`, `updateData`, `dynamics`,
`transmit`, `receive`, `process`, `processComponents`) tem corpo não-vazio e em que arquivo:linha.
**Cuidado com falsos "ativo"**: um corpo de uma linha só (`BaseClass::process(dt);`) conta como
"não-vazio" para o extrator, mas pode ser um passthrough trivial — confira o `obc`
(`OnboardComputer`) no `SCENARIO` como exemplo de nota que documenta essa armadilha.

### `TRACES.quadro` / `TRACES.fundo` / `TRACES.reset`

Cada um é um array de passos (função `step(o)`):

```js
{
  kind: 'visit' | 'phase' | 'rf' | 'resolve' | 'spawn' | 'despawn',
  node: 'tws',                  // id em SCENARIO — nó destacado
  to: 'twsTrkMgr',               // id destino, para arestas de evento (rf/resolve)
  counters: { cycle, frame, phase, exec, simT },
  dt: 0.02,
  src: 'Radar::process',         // chave em SRC
  hl: [325, 344],                 // intervalo de linhas REAIS destacado
  stack: [{ label, node }, ...],
  runs: true, idle: false,
  title: '...', body: '...', warn: '...',
  spawn: 'decoy_flyout',          // id que passa a existir a partir deste passo
  despawn: 'decoy_flyout',        // id que deixa de existir a partir deste passo
}
```

`spawn`/`despawn` são a extensão que fecha o requisito "a árvore muda em execução": a UI mantém
um conjunto de ids "existentes" por cadeia, calculado varrendo os passos vistos até o índice
atual (`computeExistence`) — nós com `dynamic:true` nascem dashed/translúcidos e viram sólidos
no passo com `spawn` igual ao seu id (e vice-versa para `despawn`). A cadeia (c) trata
`decoy_flyout` como "já existente" desde o passo 0 (não tem `spawn` nela, só `despawn`) — o
raciocínio é `computeDefaultDynamicBorn()`: um id com `despawn` e sem `spawn` NA MESMA cadeia é
tratado como nascido antes do início dela.

**Por que os geradores não enumeram TODO player em TODA fase**: falcon1 sozinho tem ~50 nós, e
falcon2/3/4/bandit1 repetem o MESMO mecanismo (`System::updateTC`, dt4). Cada trace visita
falcon1 em detalhe e condensa os demais numa única nota "idem" — decisão deliberada para o
trace caber numa barra de rolagem legível, documentada nos comentários de `buildFrameTrace()`.

**Para acrescentar um passo**: chame `step({...})` dentro do gerador certo; valide as
referências rodando o smoke-test abaixo (verifica que todo `node`/`to`/`spawn`/`despawn` existe
em `SCENARIO` e que todo `src`/`hl` bate com uma linha real de `SRC`):

```js
node -e "
$(sed -n '/^const SRC = {/,/^};$/p' index.html)
$(sed -n '/^const SCENARIO = /,/^});$/p' index.html)
$(sed -n '/^function step/,/^};$/p' index.html)
const ids = new Set(); (function w(n){ids.add(n.id); (n.children||[]).forEach(w);})(SCENARIO);
let errs = 0;
for (const [chainName, arr] of Object.entries(TRACES)) arr.forEach((s,i) => {
  for (const k of ['node','to','spawn','despawn']) if (s[k] && !ids.has(s[k])) { console.log('BAD',chainName,i,k,s[k]); errs++; }
  if (s.src && !SRC[s.src]) { console.log('BAD SRC',chainName,i,s.src); errs++; }
  if (s.src && s.hl) { const lns = flattenSrc(SRC[s.src]).map(l=>l.ln); if(!lns.includes(s.hl[0])||!lns.includes(s.hl[1])) { console.log('BAD HL',chainName,i,s.src,s.hl); errs++; } }
});
console.log('errors:', errs);
"
```

## `scripts/extract_execution_chain.py`

O extrator (Fase 1 do TODO — o `docs/TODO.md` original presumia um `scripts/check-edl.py`
existente para "reaproveitar"; ele não existe neste repositório, então este é um script novo).
Sem dependências, varre `contexts/src/mixr/`, `models/` e `shared/` por
`DECLARE_SUBCLASS`/`IMPLEMENT_*SUBCLASS` (cadeia de herança e nome de fábrica) e por definições
`Classe::método(` dos 7 métodos-alvo, casando chaves por profundidade de `{`/`}` sobre texto com
comentários mascarados (para não confundir uma MENÇÃO em comentário com uma definição real —
armadilha real, encontrada rodando: `models/flight/src/xnative/FlightAgentTC.cpp` cita
`Player::updateData()` num comentário de documentação).

```
python3 scripts/extract_execution_chain.py                 # tabela contra o cenario built-in_mixr_1
python3 scripts/extract_execution_chain.py Aircraft Gimbal  # classes/fabricas especificas
python3 scripts/extract_execution_chain.py --json           # saida em JSON
```

Um nó é **OCIOSO** quando nenhum nível da cadeia, ACIMA de `Object`/`Component`/`System` (que só
fornecem despacho genérico — inclusive os stubs vazios de `System::dynamics/transmit/receive/
process`, medidos em `System.cpp:135-148`), sobrescreve algum dos 7 métodos com corpo não-vazio.
É uma aproximação estática (não sabe se o corpo faz algo *interessante*, só se faz *alguma
coisa*) — mas é honesta, e verificável por qualquer um rodando o mesmo comando.

## O que mudou em relação ao `docs/TODO.md` original

Antes de implementar, verifiquei cada premissa do TODO contra o estado real do repositório:

1. **Caminhos errados**: `docs/MIXR-CONTEXT.md`/`docs/MIXR-PATTERN-CONTEXT.md` não existem —
   os arquivos reais são `contexts/MIXR-CONTEXT.md`/`contexts/MIXR-PATTERN-CONTEXT.md`.
   `scripts/check-edl.py` não existe — não havia nada para "reaproveitar". `poc/*/` não existe —
   as pocs vivem em `src/poc/`.
2. **A lista de "built-ins ociosos de propósito"** (`Antenna, Navigation, Route, SigSphere,
   QuadMap, Ins, Gps`) era internamente inconsistente (o próprio TODO descreve
   `Antenna::rfTransmit()` fazendo trabalho real na fase 1) e citava uma classe
   (`QuadMap`) que **não é usada em lugar nenhum deste repositório** (o terreno aqui é
   `SrtmHgtFile` direto). O conjunto de nós ociosos usado aqui foi determinado **rodando o
   extrator**, não copiado — e o resultado é mais interessante que o esperado: `Navigation`/
   `Ins`/`Route` **não são ociosos** (fazem trabalho real em `updateData()`), só têm o resultado
   **ignorado** pelo `Autopilot` (`navMode: false`) — uma distinção que o TODO original não
   fazia e que só apareceu ao ler o fonte.
3. **A liberação de arma não acontece na fase 3 do quadro**, como o TODO presumia
   ("fase 3 do segundo quadro: `Stores::releaseWeapon()`"). Lendo `Route.cpp`/
   `OnboardComputer.cpp` de verdade: `Route::autoSequencer()` roda em `Route::updateData()`
   (fundo), e quem chama `Action::process()` até o release é
   `OnboardComputer::actionManager()`, também chamado só por `OnboardComputer::updateData()`
   (fundo) — nunca pelo `switch(phase)` de um `System` (`Action` nem é um `System`: é um
   `base::ubf::AbstractAction`). Por isso a liberação do decoy mora na cadeia **(b)**, não (a),
   neste explorador — uma correção que só surge lendo o fonte, não supondo a partir da
   descrição do TODO.
4. **O mecanismo de liberação em si** também é mais preciso do que "a arma sai do cabide":
   `AbstractWeapon::release()` **clona** o docked (`st8`), dá um ID novo, e chama
   `sim->addNewPlayer()` só com o CLONE — o original nunca sai de `Stores::storesList`, só
   ganha a flag `LAUNCHED`. E `Stores::updateTC()`/`updateData()` fazem
   `dynamic_cast<ExternalStore*>` em cada item docked — para uma ARMA esse cast falha, então
   ela nunca é visitada enquanto docked (não é um "mode-gate dentro de `Player`", é um filtro
   explícito no laço do `Stores`). Nenhum destes dois fatos está no TODO original; os dois só
   apareceram lendo `Stores.cpp`/`AbstractWeapon.cpp` linha a linha.
5. **Cenário-âncora**: em vez de um cenário genérico, tudo aqui é `src/poc/built-in_mixr_1/
   configs/scenario_max_player.epp.in` — a única poc deste repositório que já existe
   *precisamente* para provar "boa parte dos componentes built-in nunca executa nada por fase",
   e que também contém, no mesmo arquivo, a liberação dinâmica de arma com timestamps medidos
   (`t=37,0s`/`t=225,3s`).
6. **Entrega em HTML único** (React via CDN), não Vite+React — o repositório inteiro não tem
   nenhuma toolchain JS hoje; criar uma para uma página de documentação contradiria o princípio,
   já demonstrado em `CLAUDE.md`, de não introduzir dependência sem necessidade. O TODO já
   autorizava esse fallback explicitamente.

## Verificado rodando (não só lendo)

- `python3 scripts/extract_execution_chain.py` roda sem dependências e sem erros.
- `SCENARIO` tem 112 nós, todos com `id` único, e todo `srcKeys` referenciado existe em `SRC`
  (checado com o script de validação acima).
- Os três `TRACES` (73 passos no total) têm 100% das referências `node`/`to`/`spawn`/`despawn`/
  `src`/`hl` válidas contra `SCENARIO`/`SRC`.
- A página renderiza em Chrome headless com conteúdo real (não uma tela em branco) — testado nas
  três abas: cadeia (a) mostra `Station::tcFrame` com a árvore auto-rolada até o nó ativo;
  cadeia (b), passo 10/15, mostra `OnboardComputer::triggerAction` com o trecho de fonte
  corretamente destacado; cadeia (c), passo 6/7, mostra `decoy_flyout` desaparecendo com a
  explicação certa e nenhum trecho de fonte associado (correto — é uma AUSÊNCIA, não uma
  chamada).
- Nenhum nome de classe/fábrica/slot usado em `SCENARIO` foi inventado — todos batem com
  `scenario_max_player.epp.in` e com a saída do extrator.

## Extensão prevista (não implementada — o gancho para ela)

O passo seguinte, descrito no `docs/TODO.md` original, é instrumentar `Component::tcFrame()` no
fork para emitir `(execCounter, phase, ponteiro, caminho na árvore, dt, entrada/saída)` num
buffer circular, despejado no encerramento em Chrome Trace Format. Um leitor desse JSON
substituiria só o bloco `TRACES` (e o `SCENARIO`, se a árvore também vier do dump) — a fronteira
entre dados e UI já está desenhada exatamente para isso: `App` nunca importa uma regra do MIXR,
só os campos `node`/`src`/`hl`/`counters`/`stack` dos passos. Trocar `buildFrameTrace()` por
`fetch('trace.json')` (ou, para manter "sem servidor", um `<script>` que injeta o JSON como
`const TRACES = {...}` gerado por um passo de build à parte) não exigiria tocar em `Tree`,
`CodePanel` nem nos painéis laterais.
