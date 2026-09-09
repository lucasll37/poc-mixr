// Conteudo das slides da apresentacao "Refatoracao do ASA" -- EDITE O TEXTO AQUI.
//
// Cada item deste array vira um <section class="slide <extraClass>"
// data-title="<title>"> dentro de #deck -- o script logo apos a div.deck, em
// index.html, monta as 30 slides a partir daqui (nesta ORDEM) antes de tudo o
// mais rodar. Adicionar/remover um item deste array adiciona/remove um slide da
// apresentacao -- nao precisa mexer em index.html para isso.
//
// 'html' e' o CONTEUDO INTERNO do slide, como texto normal (template literal --
// aspas simples/duplas nao precisam de escape, so evite usar um crase ` ou um
// ${...} dentro do texto). Pode conter qualquer marcacao ja usada nas outras
// slides (<b>, <code>, <em>, <p class="eyebrow">, <figure class="diagram">,
// diagramas SVG, video de demo...) -- e' a mesma coisa que estaria direto num
// arquivo .html.
window.PRESENTATION_SLIDES = [
  {
    extraClass: 'slide--title',
    title: 'Abertura',
    html: `
  <p class="eyebrow">Reestruturação de plataforma · MIXR + BehaviorTree.CPP</p>
  <h1>Refatoração do ASA</h1>
  <p class="lede">Simulação aeroespacial é difícil por natureza — isso não muda. O que muda é
  <u style="text-decoration:none;border-bottom:5px solid var(--accent)">tudo o que atrapalha</u>
  ao redor disso.</p>
  <div class="tag-row">
    <span class="tag">A-4 Skyhawk (produção)</span><span class="tag">onnx-policy</span>
    <span class="tag">python-flight</span><span class="tag">TOUR.md</span>
  </div>
  <p class="meta-row">
    <span><b>MIXR</b> 1.0.5</span><span><b>BehaviorTree.CPP</b> 3.5.6</span>
    <span><b>Toolchain</b> Conan → Meson/Ninja → Makefile</span>
  </p>
`
  },
  {
    extraClass: '',
    title: 'Um domínio com agentes que interagem',
    html: `
  <p class="eyebrow">I · O problema que nenhuma ferramenta resolve</p>
  <h2 class="headline">Um domínio com agentes que interagem</h2>
  <ul class="list">
    <li>Aeronaves, sensores, armas, sistemas de navegação — cada um com <b>dinâmica própria</b>,
    reagindo e causando efeitos uns nos outros em tempo real.</li>
    <li>Tudo isso ao mesmo tempo, com <b>dependências cruzadas</b> — não é dificuldade de código
    mal escrito, é dificuldade <b>inerente ao domínio</b>.</li>
  </ul>
  <figure class="diagram">
    <svg viewBox="0 0 900 440" style="max-height:38vh" role="img" aria-label="Radar, piloto automatico, alerta e terreno dispostos em circulo, cada um ligado a todos os outros -- nao uma cadeia, uma teia de dependencias cruzadas">
      <rect x="330" y="5" width="220" height="72" rx="7" fill="none" stroke="currentColor" stroke-width="1.6" class="svg-teal"/>
      <text x="440" y="36" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="14" font-weight="600" class="svg-teal">radar</text>
      <text x="440" y="56" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="11" class="svg-teal" opacity=".85">detecta</text>

      <rect x="650" y="182" width="220" height="72" rx="7" fill="none" stroke="currentColor" stroke-width="1.6" class="svg-accent"/>
      <text x="760" y="213" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="14" font-weight="600" class="svg-accent">piloto automático</text>
      <text x="760" y="233" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="11" class="svg-accent" opacity=".85">reage</text>

      <rect x="330" y="360" width="220" height="72" rx="7" fill="none" stroke="currentColor" stroke-width="1.6" class="svg-amber"/>
      <text x="440" y="391" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="14" font-weight="600" class="svg-amber">alerta</text>
      <text x="440" y="411" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="11" class="svg-amber" opacity=".85">se propaga</text>

      <rect x="10" y="182" width="220" height="72" rx="7" fill="none" stroke="currentColor" stroke-width="1.6" class="svg-brick"/>
      <text x="120" y="213" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="14" font-weight="600" class="svg-brick">terreno</text>
      <text x="120" y="233" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="11" class="svg-brick" opacity=".85">impõe piso</text>

      <line x1="500" y1="72" x2="680" y2="202" stroke="currentColor" stroke-width="1.4" class="svg-ink" opacity=".55" marker-end="url(#arrow)" marker-start="url(#arrow)"/>
      <line x1="680" y1="234" x2="500" y2="362" stroke="currentColor" stroke-width="1.4" class="svg-ink" opacity=".55" marker-end="url(#arrow)" marker-start="url(#arrow)"/>
      <line x1="380" y1="362" x2="200" y2="234" stroke="currentColor" stroke-width="1.4" class="svg-ink" opacity=".55" marker-end="url(#arrow)" marker-start="url(#arrow)"/>
      <line x1="200" y1="202" x2="380" y2="72" stroke="currentColor" stroke-width="1.4" class="svg-ink" opacity=".55" marker-end="url(#arrow)" marker-start="url(#arrow)"/>
      <line x1="440" y1="77" x2="440" y2="360" stroke="currentColor" stroke-width="1.2" stroke-dasharray="3 4" class="svg-ink" opacity=".4" marker-end="url(#arrow)" marker-start="url(#arrow)"/>
      <line x1="650" y1="218" x2="230" y2="218" stroke="currentColor" stroke-width="1.2" stroke-dasharray="3 4" class="svg-ink" opacity=".4" marker-end="url(#arrow)" marker-start="url(#arrow)"/>
    </svg>
    <figcaption>Não é uma cadeia, é uma teia: cada elemento afeta e é afetado pelos outros —
    radar, piloto automático, alerta e terreno, decidindo em paralelo, no mesmo frame.</figcaption>
  </figure>
`
  },
  {
    extraClass: '',
    title: 'O trabalho real: revisitar tudo que já existe',
    html: `
  <p class="eyebrow">I · O problema que nenhuma ferramenta resolve</p>
  <h2 class="headline">O trabalho real: revisitar tudo que já existe</h2>
  <figure class="diagram">
    <svg viewBox="0 0 800 370" style="max-height:39vh" role="img" aria-label="Um modelo novo se conecta, nos dois sentidos, com cada um dos modelos que ja existem: chaff, sistema de alerta, aeronave">
      <rect x="20" y="143" width="240" height="83" rx="8" fill="none" stroke="currentColor" stroke-width="1.8" class="svg-accent"/>
      <text x="140" y="180" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="14" font-weight="600" class="svg-accent">modelo novo</text>
      <text x="140" y="200" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="10" class="svg-accent" opacity=".85">míssil · aeronave · sensor</text>

      <rect x="480" y="14" width="280" height="74" rx="8" fill="none" stroke="currentColor" stroke-width="1.5" class="svg-amber"/>
      <text x="620" y="46" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="13" font-weight="600" class="svg-amber">chaff</text>
      <text x="620" y="66" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="10" class="svg-amber" opacity=".85">já existe</text>

      <rect x="480" y="148" width="280" height="74" rx="8" fill="none" stroke="currentColor" stroke-width="1.5" class="svg-brick"/>
      <text x="620" y="180" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="13" font-weight="600" class="svg-brick">sistema de alerta</text>
      <text x="620" y="200" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="10" class="svg-brick" opacity=".85">já existe</text>

      <rect x="480" y="282" width="280" height="74" rx="8" fill="none" stroke="currentColor" stroke-width="1.5" class="svg-teal"/>
      <text x="620" y="314" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="13" font-weight="600" class="svg-teal">aeronave</text>
      <text x="620" y="334" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="10" class="svg-teal" opacity=".85">já existe</text>

      <line x1="260" y1="162" x2="478" y2="51" stroke="currentColor" stroke-width="1.4" class="svg-ink" opacity=".6" marker-end="url(#arrow)" marker-start="url(#arrow)"/>
      <line x1="260" y1="185" x2="478" y2="185" stroke="currentColor" stroke-width="1.4" class="svg-ink" opacity=".6" marker-end="url(#arrow)" marker-start="url(#arrow)"/>
      <line x1="260" y1="207" x2="478" y2="319" stroke="currentColor" stroke-width="1.4" class="svg-ink" opacity=".6" marker-end="url(#arrow)" marker-start="url(#arrow)"/>
    </svg>
    <figcaption>Um míssil novo precisa reagir certo a um chaff que já existia — e ser
    reconhecido certo por um sistema de alerta e por outras aeronaves escritos antes dele
    existir. O efeito vale nos dois sentidos, para cada um.</figcaption>
  </figure>
  <div class="callout">
    <span class="k">Ponto central</span>
    <p>Isso não é acidental — é o próprio significado de simular um domínio com agentes que
    interagem.</p>
  </div>
`
  },
  {
    extraClass: '',
    title: 'MIXR ajuda — mas só se usado direito',
    html: `
  <p class="eyebrow">I · O problema que nenhuma ferramenta resolve</p>
  <h2 class="headline">MIXR ajuda — mas só se usado direito</h2>
  <div class="grid-2">
    <ul class="list">
      <li>O framework já vem com uma <b>dinâmica de eventos</b> e classes-base pensadas para
      resolver esse acoplamento sem que cada par de modelos precise se conhecer diretamente.</li>
      <li>Mas isso só funciona se o framework for usado <b>do jeito que foi desenhado para ser
      usado</b>.</li>
      <li>Exige entender as abstrações, o vocabulário, o uso idiomático — um MIXR mal utilizado
      não entrega nada disso de graça.</li>
    </ul>
    <figure class="diagram">
      <svg viewBox="0 0 500 400" role="img" aria-label="Modelo A e Modelo B nao se conhecem diretamente -- os dois se comunicam, nos dois sentidos, so atraves da dinamica de eventos do MIXR">
        <rect x="110" y="15" width="280" height="75" rx="7" fill="none" stroke="currentColor" stroke-width="1.5" class="svg-teal"/>
        <text x="250" y="48" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="14" font-weight="600" class="svg-teal">modelo A</text>
        <text x="250" y="68" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="10.5" class="svg-teal" opacity=".85">não conhece B</text>

        <rect x="90" y="155" width="320" height="95" rx="8" fill="none" stroke="currentColor" stroke-width="1.8" class="svg-accent"/>
        <text x="250" y="193" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="14" font-weight="600" class="svg-accent">MIXR</text>
        <text x="250" y="213" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="10.5" class="svg-accent" opacity=".85">dinâmica de eventos</text>

        <rect x="110" y="310" width="280" height="75" rx="7" fill="none" stroke="currentColor" stroke-width="1.5" class="svg-teal"/>
        <text x="250" y="343" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="14" font-weight="600" class="svg-teal">modelo B</text>
        <text x="250" y="363" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="10.5" class="svg-teal" opacity=".85">não conhece A</text>

        <line x1="250" y1="90" x2="250" y2="153" stroke="currentColor" stroke-width="1.4" stroke-dasharray="3 4" class="svg-ink" opacity=".6" marker-end="url(#arrow)" marker-start="url(#arrow)"/>
        <line x1="250" y1="250" x2="250" y2="308" stroke="currentColor" stroke-width="1.4" stroke-dasharray="3 4" class="svg-ink" opacity=".6" marker-end="url(#arrow)" marker-start="url(#arrow)"/>
      </svg>
      <figcaption>A e B nunca se conhecem diretamente — toda comunicação passa pela dinâmica de
      eventos do MIXR, nos dois sentidos.</figcaption>
    </figure>
  </div>
`
  },
  {
    extraClass: 'slide--statement',
    title: 'A dificuldade de domínio é indissociável',
    html: `
  <p class="eyebrow">I · O problema que nenhuma ferramenta resolve</p>
  <p class="statement">A proficiência em modelagem — física, comportamento, interações do
  domínio — é uma barreira que <em>nenhuma</em> ferramenta, processo ou framework vai amenizar.
  Ela é intrínseca ao que estamos simulando.</p>
  <p class="lede muted" style="margin-top:4px">O que dá para fazer: garantir que tudo o que NÃO
  é essa dificuldade intrínseca pare de atrapalhar.</p>
`
  },
  {
    extraClass: '',
    title: 'A proposta',
    html: `
  <p class="eyebrow">II · A proposta</p>
  <h2 class="headline">Uma reestruturação para devolver produtividade e escala</h2>
  <p class="lede muted">A expectativa não é remover a dificuldade de modelar — é recuperar
  produtividade e capacidade de escalar, atacando todo atrito evitável.</p>
  <ul class="list tight">
    <li><b>Ferramentas usadas do jeito certo</b> — MIXR, BehaviorTree.CPP, Groot</li>
    <li><b>Um repositório só</b>, não vários espalhados</li>
    <li><b>Esteira de CI forte</b> — testes em camadas, Makefile claro</li>
    <li><b>Contrato forte de modelo</b> — <code>docs/</code>, <code>CHANGELOG.md</code></li>
    <li><b>Ferramentas de assistência</b> — painel ao vivo, manual, editor visual</li>
    <li><b>Documentação com apoio de IA</b> + filosofia <b>poc-first</b></li>
    <li><b>Ambiente padronizado</b> — VSCode, <code>clangd</code></li>
    <li><b>Roadmap</b> — integração com o asa-engine</li>
  </ul>
`
  },
  {
    extraClass: '',
    title: 'Ferramentas usadas do jeito certo',
    html: `
  <p class="eyebrow">II · A proposta</p>
  <h2 class="headline">Ferramentas usadas do jeito que foram desenhadas</h2>
  <div class="grid-2">
    <ul class="list">
      <li>MIXR e BehaviorTree.CPP não são bibliotecas genéricas — são frameworks com
      <b>opiniões fortes</b> sobre como o código deveria se organizar.</li>
      <li>A aposta é usá-los como foram desenhados, em vez de reconstruir por fora o que eles já
      resolvem por dentro — é isso que faz a dinâmica de eventos do MIXR funcionar a nosso
      favor.</li>
      <li><b>Groot</b> entra como a ferramenta de edição e monitoramento das árvores de
      comportamento — visual, não só texto.</li>
      <li>Extensão VSCode para <code>.edl</code>: realce de sintaxe <b>e</b> validação de
      verdade — o mesmo parser do framework, antes de tentar rodar. <code>clangd</code> no lugar
      do IntelliSense padrão para C++.</li>
    </ul>
    <div class="media-placeholder">
      <svg width="30" height="30"><use href="#i-video"></use></svg>
      <span class="tag">Vídeo sugerido</span>
      <p class="caption">Groot editando uma árvore de comportamento ao vivo</p>
    </div>
  </div>
`
  },
  {
    extraClass: '',
    title: 'Um repositório, não vários',
    html: `
  <p class="eyebrow">II · A proposta</p>
  <h2 class="headline">Um repositório, não vários</h2>
  <p class="lede muted"><code>asa-models</code>, <code>asa-libs</code>, <code>asa-models-r</code>
  e o próprio MIXR — cada um com seu ciclo, sua forma de versionar, sua distância dos outros.
  Hoje, tudo em um repositório integrado: a diferença entre enxergar o efeito de uma mudança de
  ponta a ponta, ou não.</p>
  <div class="media-placeholder" style="width:100%; flex:1; min-height:0;">
    <svg width="36" height="36"><use href="#i-image"></use></svg>
    <span class="tag">Imagem sugerida</span>
    <p class="caption">Screenshot: o repositório poc-mixr integrado — um único histórico de
    git, uma única árvore de pastas, sem asa-models/asa-libs/asa-models-r espalhados</p>
  </div>
`
  },
  {
    extraClass: '',
    title: 'Uma esteira de CI forte',
    html: `
  <p class="eyebrow">II · A proposta</p>
  <h2 class="headline">Uma esteira de CI forte</h2>
  <figure class="diagram">
    <svg viewBox="0 0 980 210" role="img" aria-label="Um espectro de oito camadas de teste, da mais isolada (domain) a mais integrada (guard) -- tres do modelo, cinco do host">
      <circle cx="40" cy="22" r="5" fill="currentColor" class="svg-teal"/>
      <text x="52" y="27" text-anchor="start" fill="currentColor" font-family="IBM Plex Mono" font-size="11" class="svg-teal">modelo</text>
      <circle cx="150" cy="22" r="5" fill="currentColor" class="svg-accent"/>
      <text x="162" y="27" text-anchor="start" fill="currentColor" font-family="IBM Plex Mono" font-size="11" class="svg-accent">host</text>

      <line x1="40" y1="140" x2="930" y2="140" stroke="currentColor" stroke-width="2" class="svg-ink" opacity=".5" marker-end="url(#arrow)"/>

      <line x1="100" y1="140" x2="100" y2="124" stroke="currentColor" stroke-width="1.3" class="svg-teal" opacity=".7"/>
      <circle cx="100" cy="140" r="5" fill="currentColor" class="svg-teal"/>
      <text x="100" y="114" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="12.5" font-weight="600" class="svg-teal">domain</text>

      <line x1="211" y1="140" x2="211" y2="156" stroke="currentColor" stroke-width="1.3" class="svg-teal" opacity=".7"/>
      <circle cx="211" cy="140" r="5" fill="currentColor" class="svg-teal"/>
      <text x="211" y="172" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="12.5" font-weight="600" class="svg-teal">tree</text>

      <line x1="323" y1="140" x2="323" y2="124" stroke="currentColor" stroke-width="1.3" class="svg-teal" opacity=".7"/>
      <circle cx="323" cy="140" r="5" fill="currentColor" class="svg-teal"/>
      <text x="323" y="114" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="12.5" font-weight="600" class="svg-teal">native</text>

      <line x1="434" y1="140" x2="434" y2="156" stroke="currentColor" stroke-width="1.3" class="svg-accent" opacity=".7"/>
      <circle cx="434" cy="140" r="5" fill="currentColor" class="svg-accent"/>
      <text x="434" y="172" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="12.5" font-weight="600" class="svg-accent">scenario</text>

      <line x1="546" y1="140" x2="546" y2="124" stroke="currentColor" stroke-width="1.3" class="svg-accent" opacity=".7"/>
      <circle cx="546" cy="140" r="5" fill="currentColor" class="svg-accent"/>
      <text x="546" y="114" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="12.5" font-weight="600" class="svg-accent">memory</text>

      <line x1="657" y1="140" x2="657" y2="156" stroke="currentColor" stroke-width="1.3" class="svg-accent" opacity=".7"/>
      <circle cx="657" cy="140" r="5" fill="currentColor" class="svg-accent"/>
      <text x="657" y="172" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="12.5" font-weight="600" class="svg-accent">determinism</text>

      <line x1="769" y1="140" x2="769" y2="124" stroke="currentColor" stroke-width="1.3" class="svg-accent" opacity=".7"/>
      <circle cx="769" cy="140" r="5" fill="currentColor" class="svg-accent"/>
      <text x="769" y="114" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="12.5" font-weight="600" class="svg-accent">plugin</text>

      <line x1="880" y1="140" x2="880" y2="156" stroke="currentColor" stroke-width="1.3" class="svg-accent" opacity=".7"/>
      <circle cx="880" cy="140" r="5" fill="currentColor" class="svg-accent"/>
      <text x="880" y="172" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="12.5" font-weight="600" class="svg-accent">guard</text>

      <text x="40" y="196" text-anchor="start" fill="currentColor" font-family="IBM Plex Mono" font-size="11" class="svg-ink" opacity=".6">mais isolado</text>
      <text x="930" y="196" text-anchor="end" fill="currentColor" font-family="IBM Plex Mono" font-size="11" class="svg-ink" opacity=".6">mais integrado</text>
    </svg>
    <figcaption>Um espectro, não uma grade solta: da regra mais isolada (<code>domain</code>) ao
    contrato de plugin (<code>guard</code>) — cada camada prova uma coisa diferente, um degrau
    mais integrada que a anterior. Makefile deixa claro o que fazer em cada passo.</figcaption>
  </figure>
  <div class="stat-row">
    <span class="stat-chip">8 camadas de teste</span>
    <span class="stat-chip">1/2/4 threads: dumps byte-idênticos</span>
  </div>
  <div class="callout">
    <span class="k">O que isso compra</span>
    <p>Isso é o que transforma "eu acho que funciona" em "eu tenho como provar que funciona" —
    rastreável, sem depender de conhecimento prévio de quem já mexeu naquilo antes.</p>
  </div>
`
  },
  {
    extraClass: '',
    title: 'Um contrato forte para todo modelo',
    html: `
  <p class="eyebrow">II · A proposta</p>
  <h2 class="headline">Um contrato forte para todo modelo</h2>
  <div class="grid-2">
    <div>
      <ul class="list">
        <li>Todo modelo que entra no repositório carrega a mesma espinha dorsal:
        <b>documentação de arquitetura</b> em <code>docs/</code>.</li>
        <li>E um <code>CHANGELOG.md</code> que registra o que mudou e quando — por
        <b>data de commit</b>, nunca por data da mensagem.</li>
      </ul>
      <div class="callout" style="margin-top:18px">
        <span class="k">Por quê</span>
        <p>Mensagem de commit se engana. Data de commit não. Isso é gestão de conhecimento de
        verdade: o conhecimento não fica só na cabeça de quem escreveu — fica registrado de um
        jeito que sobrevive à saída da pessoa.</p>
      </div>
    </div>
    <div class="media-placeholder media-placeholder--fill">
      <svg width="30" height="30"><use href="#i-image"></use></svg>
      <span class="tag">Imagem sugerida</span>
      <p class="caption">Screenshot: pasta de um modelo real — tests/, docs/, README.md,
      CHANGELOG.md, Makefile</p>
    </div>
  </div>
`
  },
  {
    extraClass: '',
    title: 'Ferramentas que assistem o desenvolvimento',
    html: `
  <p class="eyebrow">II · A proposta</p>
  <h2 class="headline">Ferramentas que assistem o desenvolvimento e o debug</h2>
  <ul class="list">
    <li><code>./app</code> — painel de controle e observação em tempo real: qual comportamento
    está ativo, em qual thread, com qual estado de memória.</li>
    <li>Um <b>manual interativo</b> que navega pela estrutura do framework e pela árvore de
    decisão de verdade que os modelos usam.</li>
    <li>Um <b>editor visual</b> para montar cenários, sem precisar decorar sintaxe.</li>
  </ul>
  <div class="media-grid">
    <div class="media-placeholder">
      <svg width="28" height="28"><use href="#i-video"></use></svg>
      <span class="tag">Vídeo sugerido</span>
      <p class="caption">./app (TUI) — painel ao vivo, trocando de aba durante a simulação</p>
    </div>
    <div class="media-placeholder">
      <svg width="28" height="28"><use href="#i-image"></use></svg>
      <span class="tag">Imagem sugerida</span>
      <p class="caption">Manual interativo navegando a árvore de decisão</p>
    </div>
    <div class="media-placeholder">
      <svg width="28" height="28"><use href="#i-video"></use></svg>
      <span class="tag">Vídeo sugerido</span>
      <p class="caption">Editor visual montando um cenário arrastando classes</p>
    </div>
  </div>
`
  },
  {
    extraClass: '',
    title: 'Documentação com IA + poc-first',
    html: `
  <p class="eyebrow">II · A proposta</p>
  <h2 class="headline">Documentação com apoio de IA + a filosofia poc-first</h2>
  <div class="grid-2">
    <ul class="list">
      <li>Não é terceirizar a explicação para uma IA e aceitar o que sai.</li>
      <li>É usar IA como <b>alavanca</b> para produzir e manter atualizada uma documentação que,
      historicamente, é a primeira coisa a envelhecer mal em qualquer projeto de engenharia.</li>
    </ul>
    <ul class="list">
      <li>Em vez de desenhar a arquitetura ideal no papel antes de escrever qualquer linha, a
      aposta é começar simples, com uma prova de conceito pequena.</li>
    </ul>
  </div>
  <p class="eyebrow" style="margin-top:8px">poc-first, na prática</p>
  <div class="pipeline">
    <span class="pipe-step">poc pequena</span><span class="pipe-arrow">→</span>
    <span class="pipe-step">vira <b>contrato</b></span><span class="pipe-arrow">→</span>
    <span class="pipe-step">vira <b>exemplo de como fazer</b></span><span class="pipe-arrow">→</span>
    <span class="pipe-step">próximo modelo copia e adapta</span>
  </div>
  <p class="lede muted">Um caminho já percorrido para copiar e adaptar — não um documento de
  intenções.</p>
`
  },
  {
    extraClass: '',
    title: 'Olhando para frente: o asa-engine',
    html: `
  <div class="kicker-row">
    <p class="eyebrow">II · A proposta</p>
    <span class="feat-badge">Roadmap — <b>ainda não construído</b></span>
  </div>
  <h2 class="headline">Olhando para frente: integração com o asa-engine</h2>
  <figure class="diagram">
    <svg viewBox="0 0 880 300" role="img" aria-label="poc-mixr hoje se conecta ao asa-engine, que se conecta a asa-py, webstation e, como exploracao futura ainda mais adiante, ao FlightGear">
      <rect x="20" y="90" width="170" height="70" rx="6" fill="none" stroke="currentColor" stroke-width="1.5" class="svg-ink"/>
      <text x="105" y="122" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="12" font-weight="600" class="svg-ink">poc-mixr</text>
      <text x="105" y="140" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="10.5" class="svg-ink" opacity=".7">hoje</text>

      <rect x="260" y="80" width="230" height="90" rx="6" fill="none" stroke="currentColor" stroke-width="1.5" class="svg-teal"/>
      <text x="375" y="115" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="12" font-weight="600" class="svg-teal">asa-engine</text>
      <text x="375" y="133" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="10" class="svg-teal" opacity=".8">node · manager</text>
      <text x="375" y="148" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="10" class="svg-teal" opacity=".8">handler</text>

      <rect x="610" y="10" width="220" height="64" rx="6" fill="none" stroke="currentColor" stroke-width="1.4" class="svg-accent"/>
      <text x="720" y="47" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="12" class="svg-accent">asa-py</text>

      <rect x="610" y="118" width="220" height="64" rx="6" fill="none" stroke="currentColor" stroke-width="1.3" stroke-dasharray="4 4" class="svg-ink" opacity=".7"/>
      <text x="720" y="145" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="12" font-weight="600" class="svg-ink">FlightGear</text>
      <text x="720" y="163" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="9.5" class="svg-ink" opacity=".75">(exploração futura)</text>

      <rect x="610" y="226" width="220" height="64" rx="6" fill="none" stroke="currentColor" stroke-width="1.4" class="svg-accent"/>
      <text x="720" y="263" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="12" class="svg-accent">webstation</text>

      <line x1="192" y1="125" x2="258" y2="125" stroke="currentColor" stroke-width="1.4" stroke-dasharray="3 4" class="svg-ink" opacity=".6" marker-end="url(#arrow)"/>
      <line x1="492" y1="102" x2="608" y2="45" stroke="currentColor" stroke-width="1.3" stroke-dasharray="3 4" class="svg-ink" opacity=".55" marker-end="url(#arrow)"/>
      <line x1="492" y1="125" x2="608" y2="150" stroke="currentColor" stroke-width="1.3" stroke-dasharray="3 4" class="svg-ink" opacity=".5" marker-end="url(#arrow)"/>
      <line x1="492" y1="148" x2="608" y2="255" stroke="currentColor" stroke-width="1.3" stroke-dasharray="3 4" class="svg-ink" opacity=".55" marker-end="url(#arrow)"/>
    </svg>
    <figcaption>Isso é roadmap, não algo pronto neste repositório hoje. Mas é a razão pela qual
    a reestruturação foi desenhada assim — para que, quando a integração acontecer, ela encontre
    uma base organizada, não repositórios espalhados para costurar. FlightGear entra como uma
    exploração ainda mais adiante — visualização 3D completa, além do que o Tacview já cobre
    hoje.</figcaption>
  </figure>
`
  },
  {
    extraClass: 'slide--demo',
    title: '1. ./app — todas as telas possíveis',
    html: `
  <p class="eyebrow">Demonstrações em vídeo</p>
  <h2 class="headline">1. ./app — todas as telas possíveis</h2>
  <div class="demo-stage">
    <video controls playsinline src="demos/01-app.mp4"></video>
  </div>
`
  },
  {
    extraClass: 'slide--demo',
    title: '2. Documentação iterativa',
    html: `
  <p class="eyebrow">Demonstrações em vídeo</p>
  <h2 class="headline">2. Documentação iterativa</h2>
  <div class="demo-stage">
    <video controls playsinline src="demos/02-documentacao-iterativa.mp4"></video>
  </div>
`
  },
  {
    extraClass: 'slide--demo',
    title: '3. Editor minimalista de EDL',
    html: `
  <p class="eyebrow">Demonstrações em vídeo</p>
  <h2 class="headline">3. Editor minimalista de EDL</h2>
  <div class="demo-stage" data-demo="demos/03-editor-edl">
    <video controls playsinline hidden></video>
    <div class="demo-empty">
      <svg width="34" height="34"><use href="#i-video"></use></svg>
      <span class="tag">Vídeo ainda não adicionado</span>
      <p class="caption">3. Editor minimalista de EDL</p>
      <p class="hint">coloque "03-editor-edl.mp4" (ou .webm/.mov) em docs/presentation/demos/</p>
    </div>
  </div>
`
  },
  {
    extraClass: 'slide--demo',
    title: '4. Esteira de build',
    html: `
  <p class="eyebrow">Demonstrações em vídeo</p>
  <h2 class="headline">4. Esteira de build — clean → configure → models → build → install</h2>
  <div class="demo-stage">
    <video controls playsinline src="demos/04-esteira-build.mp4"></video>
  </div>
`
  },
  {
    extraClass: 'slide--demo',
    title: '5. Criação de modelo + árvore',
    html: `
  <p class="eyebrow">Demonstrações em vídeo</p>
  <h2 class="headline">5. Criação de modelo com make + geração da árvore</h2>
  <div class="demo-stage" data-demo="demos/05-criacao-modelo">
    <video controls playsinline hidden></video>
    <div class="demo-empty">
      <svg width="34" height="34"><use href="#i-video"></use></svg>
      <span class="tag">Vídeo ainda não adicionado</span>
      <p class="caption">5. Criação de modelo com make + geração da árvore</p>
      <p class="hint">coloque "05-criacao-modelo.mp4" (ou .webm/.mov) em docs/presentation/demos/</p>
    </div>
  </div>
`
  },
  {
    extraClass: 'slide--demo',
    title: '6. Groot acompanhando a simulação',
    html: `
  <p class="eyebrow">Demonstrações em vídeo</p>
  <h2 class="headline">6. Groot acompanhando a simulação</h2>
  <div class="demo-stage" data-demo="demos/06-groot">
    <video controls playsinline hidden></video>
    <div class="demo-empty">
      <svg width="34" height="34"><use href="#i-video"></use></svg>
      <span class="tag">Vídeo ainda não adicionado</span>
      <p class="caption">6. Groot acompanhando a simulação</p>
      <p class="hint">coloque "06-groot.mp4" (ou .webm/.mov) em docs/presentation/demos/</p>
    </div>
  </div>
`
  },
  {
    extraClass: 'slide--demo',
    title: '7. Catálogo de modelos + built-in',
    html: `
  <p class="eyebrow">Demonstrações em vídeo</p>
  <h2 class="headline">7. Catálogo de modelos e modelos built-in</h2>
  <div class="demo-stage">
    <video controls playsinline src="demos/07-catalogo-modelos.mp4"></video>
  </div>
`
  },
  {
    extraClass: 'slide--demo',
    title: '8. Mapa de documentação',
    html: `
  <p class="eyebrow">Demonstrações em vídeo</p>
  <h2 class="headline">8. Mapa de documentação</h2>
  <div class="demo-stage" data-demo="demos/08-mapa-documentacao">
    <video controls playsinline hidden></video>
    <div class="demo-empty">
      <svg width="34" height="34"><use href="#i-video"></use></svg>
      <span class="tag">Vídeo ainda não adicionado</span>
      <p class="caption">8. Mapa de documentação</p>
      <p class="hint">coloque "08-mapa-documentacao.mp4" (ou .webm/.mov) em docs/presentation/demos/</p>
    </div>
  </div>
`
  },
  {
    extraClass: 'slide--demo',
    title: '9. PoCs de RL e script Python',
    html: `
  <p class="eyebrow">Demonstrações em vídeo</p>
  <h2 class="headline">9. PoCs de RL e script Python</h2>
  <div class="demo-stage" data-demo="demos/09-rl-python">
    <video controls playsinline hidden></video>
    <div class="demo-empty">
      <svg width="34" height="34"><use href="#i-video"></use></svg>
      <span class="tag">Vídeo ainda não adicionado</span>
      <p class="caption">9. PoCs de RL e script Python</p>
      <p class="hint">coloque "09-rl-python.mp4" (ou .webm/.mov) em docs/presentation/demos/</p>
    </div>
  </div>
`
  },
  {
    extraClass: '',
    title: 'O primeiro entregável',
    html: `
  <p class="eyebrow">III · Onde estamos</p>
  <h2 class="headline">O primeiro entregável</h2>
  <div class="grid-2">
    <div>
      <ul class="list">
        <li>Um metaprojeto <b>open-source</b>, com todo o esqueleto do que pode vir a ser, no
        futuro, uma refatoração completa da ASA.</li>
        <li>Hoje, com um único modelo de produção pronto de ponta a ponta: a aeronave <b>A-4
        Skyhawk</b>.</li>
        <li>E provas de conceito que mostram até onde essa estrutura pode ir: uma decisão tomada
        por uma <b>rede neural treinada via ONNX</b>, e uma decisão escrita <b>inteiramente em
        Python</b>, sem recompilar nada.</li>
      </ul>
      <div class="stat-row">
        <span class="stat-chip">MLP 28→64→64→3, 6.211 parâmetros (ONNX)</span>
        <span class="stat-chip">~42 µs/decisão (Python)</span>
      </div>
    </div>
    <div class="media-placeholder media-placeholder--fill">
      <svg width="30" height="30"><use href="#i-image"></use></svg>
      <span class="tag">Imagem sugerida</span>
      <p class="caption">Screenshot: Tacview mostrando a A-4 Skyhawk em voo, com os falcons e o
      intruso</p>
    </div>
  </div>
`
  },
  {
    extraClass: '',
    title: 'A árvore de comportamento é um contrato inegociável',
    html: `
  <p class="eyebrow">III · Onde estamos</p>
  <h2 class="headline">A árvore de comportamento é um contrato inegociável</h2>
  <p class="lede muted">A estrutura de decisão é sempre a mesma — o que varia é o que cada
  folha pode ser: um nó em C++, um script Python interpretado no frame, ou uma política ONNX
  treinada, como folha única.</p>
  <div class="media-placeholder" style="width:100%; flex:1; min-height:0;">
    <svg width="36" height="36"><use href="#i-video"></use></svg>
    <span class="tag">Vídeo sugerido</span>
    <p class="caption">A mesma árvore rodando com uma folha C++, depois trocando para Python,
    depois para uma política ONNX — sem recompilar nada</p>
  </div>
`
  },
  {
    extraClass: '',
    title: 'Por baixo da árvore: o UBF nativo do MIXR',
    html: `
  <p class="eyebrow">III · Onde estamos</p>
  <h2 class="headline">Por baixo da árvore: o UBF nativo do MIXR</h2>
  <p class="lede muted">A árvore de comportamento não é a única arquitetura de decisão em
  jogo — por baixo dela, quem a executa é um agente do <b>UBF</b> (<i>Unified Behavior
  Framework</i>), a arquitetura de decisão nativa do próprio MIXR.</p>
  <figure class="diagram">
    <svg viewBox="0 0 900 190" role="img" aria-label="Estado le o ator, Comportamento decide uma acao a partir do estado, Acao executa essa decisao sobre o ator">
      <rect x="20" y="45" width="260" height="90" rx="7" fill="none" stroke="currentColor" stroke-width="1.6" class="svg-teal"/>
      <text x="150" y="82" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="14" font-weight="600" class="svg-teal">Estado</text>
      <text x="150" y="102" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="11" class="svg-teal" opacity=".85">percepção — lê o ator</text>

      <rect x="320" y="45" width="260" height="90" rx="7" fill="none" stroke="currentColor" stroke-width="1.6" class="svg-accent"/>
      <text x="450" y="82" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="14" font-weight="600" class="svg-accent">Comportamento</text>
      <text x="450" y="102" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="11" class="svg-accent" opacity=".85">decisão — gera uma ação</text>

      <rect x="620" y="45" width="260" height="90" rx="7" fill="none" stroke="currentColor" stroke-width="1.6" class="svg-amber"/>
      <text x="750" y="82" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="14" font-weight="600" class="svg-amber">Ação</text>
      <text x="750" y="102" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="11" class="svg-amber" opacity=".85">atuação — executa no ator</text>

      <line x1="280" y1="90" x2="318" y2="90" stroke="currentColor" stroke-width="1.6" class="svg-ink" opacity=".6" marker-end="url(#arrow)"/>
      <line x1="580" y1="90" x2="618" y2="90" stroke="currentColor" stroke-width="1.6" class="svg-ink" opacity=".6" marker-end="url(#arrow)"/>
    </svg>
    <figcaption>O Agente fecha esse ciclo a cada frame: atualiza o Estado a partir do ator, pede
    uma Ação ao Comportamento, e executa essa Ação — sem que ela guarde referência nenhuma para
    o ator.</figcaption>
  </figure>
`
  },
  {
    extraClass: '',
    title: 'De Agent para AgentTC: muda a fase, muda o resultado',
    html: `
  <p class="eyebrow">III · Onde estamos</p>
  <h2 class="headline">De <code>Agent</code> para <code>AgentTC</code>: muda a fase, muda o
  resultado</h2>
  <p class="lede muted">Uma versão anterior já se aproveitava do UBF — mas herdava de
  <code>Agent</code>, não de <code>AgentTC</code>: o ciclo ficava preso à thread de fundo, fora
  do frame de tempo crítico.</p>
  <figure class="diagram">
    <svg viewBox="0 0 900 195" role="img" aria-label="As quatro fases do frame de tempo critico, com a fase 3 destacada como onde a decisao roda hoje; abaixo, updateData na thread de fundo, onde a versao anterior decidia">
      <rect x="30" y="15" width="185" height="65" rx="6" fill="none" stroke="currentColor" stroke-width="1.4" class="svg-ink" opacity=".55"/>
      <text x="122" y="45" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="13" class="svg-ink" opacity=".75">fase 0</text>
      <text x="122" y="63" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="10" class="svg-ink" opacity=".65">dynamics</text>

      <rect x="240" y="15" width="185" height="65" rx="6" fill="none" stroke="currentColor" stroke-width="1.4" class="svg-ink" opacity=".55"/>
      <text x="332" y="45" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="13" class="svg-ink" opacity=".75">fase 1</text>
      <text x="332" y="63" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="10" class="svg-ink" opacity=".65">transmit</text>

      <rect x="450" y="15" width="185" height="65" rx="6" fill="none" stroke="currentColor" stroke-width="1.4" class="svg-ink" opacity=".55"/>
      <text x="542" y="45" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="13" class="svg-ink" opacity=".75">fase 2</text>
      <text x="542" y="63" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="10" class="svg-ink" opacity=".65">receive</text>

      <rect x="660" y="15" width="185" height="65" rx="6" fill="none" stroke="currentColor" stroke-width="2" class="svg-accent"/>
      <text x="752" y="45" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="13" font-weight="600" class="svg-accent">fase 3</text>
      <text x="752" y="63" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="10" class="svg-accent" opacity=".9">process — decisão</text>

      <line x1="215" y1="48" x2="238" y2="48" stroke="currentColor" stroke-width="1.4" class="svg-ink" opacity=".5" marker-end="url(#arrow)"/>
      <line x1="425" y1="48" x2="448" y2="48" stroke="currentColor" stroke-width="1.4" class="svg-ink" opacity=".5" marker-end="url(#arrow)"/>
      <line x1="635" y1="48" x2="658" y2="48" stroke="currentColor" stroke-width="1.4" class="svg-ink" opacity=".5" marker-end="url(#arrow)"/>

      <rect x="30" y="118" width="460" height="55" rx="6" fill="none" stroke="currentColor" stroke-width="1.4" stroke-dasharray="4 4" class="svg-brick" opacity=".85"/>
      <text x="260" y="142" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="13" font-weight="600" class="svg-brick">updateData()</text>
      <text x="260" y="160" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="10" class="svg-brick" opacity=".85">fundo, ~10 Hz — versão anterior decidia aqui</text>

      <line x1="480" y1="120" x2="742" y2="83" stroke="currentColor" stroke-width="1.3" stroke-dasharray="3 4" class="svg-ink" opacity=".6" marker-end="url(#arrow)"/>
      <text x="605" y="97" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="10.5" class="svg-ink" opacity=".8">mudou para</text>
    </svg>
    <figcaption>Hoje o mesmo ciclo decide em <code>updateTC()</code>, fase 3, até 50 Hz — cada
    aeronave na própria thread do pool, com dumps byte-idênticos em 1, 2 ou 4 threads. Ganho
    maior em simulações isoladas do que em lote.</figcaption>
  </figure>
`
  },
  {
    extraClass: '',
    title: 'Cobertura garantida na infraestrutura, domínio é do modelista',
    html: `
  <p class="eyebrow">III · Onde estamos</p>
  <h2 class="headline">Cobertura garantida na infraestrutura, domínio é do modelista</h2>
  <div class="grid-2">
    <ul class="list">
      <li>As camadas de teste do <b>host</b> — cenário, memória, determinismo, plugin, guarda —
      já vêm prontas e valem para qualquer modelo que entrar no repositório, sem esforço extra.</li>
      <li>As camadas de teste do <b>modelo</b> — domínio, árvore, nativo — são a regra de
      negócio daquela aeronave especificamente, e ficam naturalmente a cargo de quem escreve o
      modelo.</li>
      <li>É exatamente aí que a IA brilha: escrever bateria de teste de regra de domínio, com
      casos de borda, é trabalho repetitivo — o tipo de trabalho onde um assistente multiplica a
      velocidade sem abrir mão de cobertura.</li>
    </ul>
    <div class="callout" style="align-self:start">
      <span class="k">Consequência prática</span>
      <p>Quem escreve um modelo novo herda de graça toda a rede de segurança do host — e
      escreve, com apoio de IA, só os testes que são realmente específicos daquele modelo.</p>
    </div>
  </div>
`
  },
  {
    extraClass: '',
    title: 'A física também é configurável — sem recompilar',
    html: `
  <p class="eyebrow">III · Onde estamos</p>
  <h2 class="headline">A física também é configurável — sem recompilar</h2>
  <div class="grid-2">
    <ul class="list">
      <li>Mesma aeronave, mesma árvore de comportamento, mesma rota — só o slot
      <code>dynamicsModel:</code> do <code>.edl</code> muda: seis graus de liberdade completos
      via <b>JSBSim</b>, um modelo linear de quatro graus via <b>LaeroModel</b>, ou um modelo
      cinemático de três canais comandados — rumo, altitude, velocidade — via
      <b>RacModel</b>.</li>
      <li>Nenhuma das três trocas exige recompilar uma linha de C++ — é configuração de
      cenário, não mudança de código. E o realce de sintaxe real dos arquivos <code>.edl</code>
      — o mesmo vocabulário que o parser da simulação entende — ajuda a explorar isso com
      confiança.</li>
    </ul>
    <div>
      <div class="stat-row" style="margin-top:0">
        <span class="stat-chip">6 DOF — JSBSimModel (produção)</span>
        <span class="stat-chip">4 DOF — LaeroModel</span>
        <span class="stat-chip">3 DOF — RacModel (cinemático)</span>
      </div>
      <div class="callout" style="margin-top:18px">
        <span class="k">Por que importa</span>
        <p>Nem toda aeronave de um cenário precisa da fidelidade completa do JSBSim o tempo
        inteiro — um modelo de dinâmica mais barato, para quem está só de pano de fundo, dá
        fôlego para escalar o número de entidades num cenário grande. Ainda em exploração, não
        em produção.</p>
      </div>
    </div>
  </div>
`
  },
  {
    extraClass: 'slide--statement',
    title: 'Reescrever é recuperar conhecimento',
    html: `
  <p class="eyebrow">III · Onde estamos</p>
  <p class="statement">Código é, por natureza, uma forma difícil de guardar conhecimento —
  mesmo em uma linguagem tão legível quanto Python. Reescrever um modelo antigo é a
  oportunidade de recuperar esse conhecimento, com documentação e testes junto.</p>
  <p class="lede muted" style="margin-top:4px">E esse é o melhor momento possível para isso —
  com IA tornando muito mais viável ler, entender e reescrever código antigo.</p>
`
  },
  {
    extraClass: '',
    title: 'Comece pelo TOUR.md',
    html: `
  <p class="eyebrow">IV · O convite</p>
  <h2 class="headline">Comece pelo TOUR.md</h2>
  <div class="grid-2">
    <ul class="list">
      <li>Um roteiro guiado, pensado para <b>não exigir conhecimento prévio</b> do repositório
      — exatamente o problema que ele existe para resolver.</li>
      <li>Leva você a subir o ambiente, rodar um cenário de verdade, ver a simulação no
      Tacview, explorar o painel de controle, abrir o manual interativo.</li>
      <li>Segunda trilha, para quem quiser ir mais fundo: o editor visual de cenários, o Groot,
      a decisão em Python, o ambiente de treino de RL.</li>
    </ul>
    <div class="media-placeholder media-placeholder--fill">
      <svg width="28" height="28"><use href="#i-video"></use></svg>
      <span class="tag">Vídeo sugerido</span>
      <p class="caption">Walkthrough: subir o ambiente, rodar um cenário, ver no Tacview</p>
    </div>
  </div>
`
  },
  {
    extraClass: '',
    title: 'Estresse o metaprojeto',
    html: `
  <p class="eyebrow">IV · O convite</p>
  <h2 class="headline">Estresse o metaprojeto</h2>
  <ul class="list">
    <li>O convite não é só "leia a documentação" — é <b>rode, quebre, proponha melhorias,
    reporte erros</b>.</li>
    <li>Amigável e plug-and-play não significa livre de erros — significa que encontrar e
    reportar esses erros já é parte esperada e valiosa do processo.</li>
    <li>Em <code>docs/books</code>: dois livros escritos com apoio de IA, um sobre o MIXR e
    outro sobre o BehaviorTree.CPP — material de capacitação para quem está entrando agora.</li>
  </ul>
  <div class="grid-2" style="align-items:center">
    <div class="callout">
      <span class="k">Por quê</span>
      <p>Um sistema assim só amadurece com gente de verdade tentando usá-lo.</p>
    </div>
    <div class="media-placeholder">
      <svg width="28" height="28"><use href="#i-image"></use></svg>
      <span class="tag">Imagem sugerida</span>
      <p class="caption">Capas dos dois livros — manual do MIXR e do BehaviorTree.CPP</p>
    </div>
  </div>
`
  },
  {
    extraClass: '',
    title: 'O que se espera',
    html: `
  <p class="eyebrow">V · O que se espera</p>
  <h2 class="headline">O que se espera</h2>
  <div class="grid-2">
    <ul class="list">
      <li>Que cada pessoa que passar por esse processo deixe esse framework com a <b>cara do que
      gostaria de trabalhar</b> no futuro.</li>
      <li>Chegar a um núcleo o mais próximo possível do <b>imutável</b> — uma base estável o
      suficiente para que o esforço de cada equipe vá para o que é específico do modelo dela.</li>
    </ul>
    <figure class="diagram">
      <svg viewBox="0 0 520 320" role="img" aria-label="Slot, plugin e folha de arvore de comportamento convergindo para um vocabulario comum">
        <rect x="20" y="10" width="220" height="60" rx="7" fill="none" stroke="currentColor" stroke-width="1.4" class="svg-teal"/>
        <text x="130" y="47" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="13" class="svg-teal">"slot"</text>

        <rect x="20" y="130" width="220" height="60" rx="7" fill="none" stroke="currentColor" stroke-width="1.4" class="svg-teal"/>
        <text x="130" y="167" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="13" class="svg-teal">"plugin"</text>

        <rect x="20" y="250" width="220" height="60" rx="7" fill="none" stroke="currentColor" stroke-width="1.4" class="svg-teal"/>
        <text x="130" y="280" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="12" class="svg-teal">"folha de árvore</text>
        <text x="130" y="296" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="12" class="svg-teal">de comportamento"</text>

        <rect x="300" y="115" width="200" height="90" rx="8" fill="none" stroke="currentColor" stroke-width="1.7" class="svg-accent"/>
        <text x="400" y="152" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="13" font-weight="600" class="svg-accent">vocabulário</text>
        <text x="400" y="172" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="13" font-weight="600" class="svg-accent">comum</text>

        <line x1="242" y1="40" x2="298" y2="130" stroke="currentColor" stroke-width="1.3" class="svg-ink" opacity=".55" marker-end="url(#arrow)"/>
        <line x1="242" y1="160" x2="298" y2="160" stroke="currentColor" stroke-width="1.3" class="svg-ink" opacity=".55" marker-end="url(#arrow)"/>
        <line x1="242" y1="280" x2="298" y2="190" stroke="currentColor" stroke-width="1.3" class="svg-ink" opacity=".55" marker-end="url(#arrow)"/>
      </svg>
      <figcaption>Quando os termos significam a mesma coisa para todo mundo, uma reunião técnica
      para de gastar tempo alinhando terminologia.</figcaption>
    </figure>
  </div>
`
  },
  {
    extraClass: 'slide--statement',
    title: 'Fechamento',
    html: `
  <p class="eyebrow">V · O que se espera</p>
  <p class="statement">Simulação aeroespacial vai continuar sendo difícil. Mas a forma como
  escrevemos, testamos, documentamos e conversamos sobre os modelos que simulam esse domínio —
  <em>essa parte está em nossas mãos</em>.</p>
  <p class="lede muted" style="margin-top:4px">Um eixo de cada vez. Um modelo, hoje. E o convite
  aberto para o próximo.</p>
`
  },
  {
    extraClass: 'slide--statement',
    title: 'Encerramento',
    html: `
  <p class="eyebrow">V · Encerramento</p>
  <p class="statement">Obrigado.</p>
  <p class="lede muted" style="margin-top:4px">poc-mixr · Refatoração do ASA — perguntas?</p>
  <div class="media-placeholder" style="width:100%; flex:1; min-height:0;">
    <svg width="40" height="40"><use href="#i-image"></use></svg>
    <span class="tag">Imagem sugerida</span>
    <p class="caption">Logo da ASA ou imagem de encerramento do time</p>
  </div>
`
  },
];
