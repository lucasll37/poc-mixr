// Conteudo das slides da apresentacao "Refatoracao do ASA" -- EDITE O TEXTO AQUI.
//
// Cada item deste array vira um <section class="slide <extraClass>"
// data-title="<title>"> dentro de #deck -- o script logo apos a div.deck, em
// index.html, monta as slides a partir daqui (nesta ORDEM) antes de tudo o
// mais rodar. Adicionar/remover um item deste array adiciona/remove um slide da
// apresentacao -- nao precisa mexer em index.html para isso.
//
// 'html' e' o CONTEUDO INTERNO do slide, como texto normal (template literal --
// aspas simples/duplas nao precisam de escape, so evite usar um crase ` ou um
// ${...} dentro do texto). Pode conter qualquer marcacao ja usada nas outras
// slides (<b>, <code>, <em>, <p class="eyebrow">, <figure class="diagram">,
// diagramas SVG, video de demo...) -- e' a mesma coisa que estaria direto num
// arquivo .html.
//
// Roteiro de origem: docs/presentation/SLIDE.md (secoes I-IV). O texto aqui NAO
// e' copia literal dele -- e' a mesma ideia, convertida no mesmo formato que ja
// vinha sendo usado (eyebrow/headline, bullets com <b> nos termos-chave,
// diagrama reaproveitado quando a ideia e' a mesma, callout para o ponto
// central, stat-row para fato quantificado, media-placeholder para sugestao de
// midia futura, slide--statement para tese forte, slide--demo para video cheio).
window.PRESENTATION_SLIDES = [
  {
    extraClass: 'slide--title',
    title: 'Abertura',
    html: `
  <p class="eyebrow">Reestruturação de plataforma · MIXR + BehaviorTree.CPP</p>
  <h1>Refatoração do ASA</h1>
  <p class="lede">Simulação aeroespacial é desafiadora por natureza — isso não muda. O que muda é
  <u style="text-decoration:none;border-bottom:5px solid var(--accent)">arquitura</u>
  ao redor disso.</p>
  <div class="tag-row">
  <!--
  <span class="tag">A-4 Skyhawk (produção)</span><span class="tag">onnx-policy</span>
  <span class="tag">python-flight</span><span class="tag">TOUR.md</span>
  </div>
  -->
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
    reagindo e causando efeito uns nos outros.</li>
    <li>Tudo isso ao mesmo tempo, com <b>dependências cruzadas</b> — não é dificuldade de código
    malfeito, é dificuldade <b>inerente ao domínio</b> que estamos simulando.</li>
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
  <p class="lede muted">Quando alguém implementa um modelo novo, o trabalho não termina em
  implementar a dinâmica dele. O trabalho real é descrever o comportamento que ele tem frente ao
  que já existe — e vice-versa.</p>
  <figure class="diagram">
    <svg viewBox="0 0 800 370" style="max-height:36vh" role="img" aria-label="Um modelo novo se conecta, nos dois sentidos, com cada um dos modelos que ja existem: chaff, sistema de alerta, aeronave">
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
    reconhecido certo por um sistema de alerta e por outras aeronaves escritos antes dele existir.
    Vale nos dois sentidos, para cada par.</figcaption>
  </figure>
  <div class="callout">
    <span class="k">Ponto central</span>
    <p>Isso não é acidental — é o próprio significado de simular um domínio com agentes que
    interagem. Implementar a dinâmica é a parte fácil.</p>
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
    <li><b>Uso idiomático das ferramentas</b> — MIXR, BehaviorTree.CPP</li>
    <li><b>Um repositório só</b>, não vários espalhados</li>
    <li><b>Esteira de CI forte</b> — testes em camadas, Makefile claro</li>
    <li><b>Contrato forte de modelo</b> — <code>docs/</code>, <code>CHANGELOG.md</code></li>
    <li><b>Ferramentas de assistência</b> — painel ao vivo, manual, editor visual</li>
    <li><b>Ambiente padronizado</b> — extensão VSCode para <code>.edl</code></li>
    <li><b>Roadmap</b> — integração com o asa-engine</li>
  </ul>
`
  },
  {
    extraClass: '',
    title: 'Ferramentas usadas do jeito que foram desenhadas',
    html: `
  <p class="eyebrow">II · A proposta</p>
  <h2 class="headline">Ferramentas usadas do jeito que foram desenhadas</h2>
  <ul class="list">
    <li>MIXR e BehaviorTree.CPP não são bibliotecas genéricas — são frameworks com
    <b>opiniões fortes</b> sobre como o código deveria se organizar.</li>
    <li>A aposta é usá-los como foram desenhados para ser usados, em vez de reconstruir por fora
    aquilo que eles já resolvem por dentro.</li>
    <li>É isso que faz a dinâmica de eventos do MIXR — a mesma que separa modelo A de modelo B
    sem que um conheça o outro — funcionar a nosso favor, de verdade.</li>
  </ul>
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
    contrato de plugin (<code>guard</code>) — cada camada prova uma coisa diferente. Makefile deixa
    claro o que fazer para configurar, compilar, testar e rodar um modelo, em cada passo.</figcaption>
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
        <p>Mensagem de commit se engana; data de commit, não. O conhecimento sobre aquele modelo
        fica registrado de um jeito que sobrevive à saída de quem escreveu.</p>
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
`
  },
  {
    extraClass: 'slide--demo',
    title: '1. ./app — painel de controle',
    html: `
  <p class="eyebrow">Demonstrações em vídeo</p>
  <h2 class="headline">1. ./app — painel de controle e observação em tempo real</h2>
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
  <h2 class="headline">2. Documentação iterativa — o manual interativo</h2>
  <div class="demo-stage">
    <video controls playsinline src="demos/02-documentacao-iterativa.mp4"></video>
  </div>
`
  },
  {
    extraClass: 'slide--demo',
    title: '3. Editor visual de cenários (EDL-builder)',
    html: `
  <p class="eyebrow">Demonstrações em vídeo</p>
  <h2 class="headline">3. Editor visual de cenários — EDL-builder</h2>
  <div class="demo-stage" data-demo="demos/03-editor-edl">
    <video controls playsinline hidden></video>
    <div class="demo-empty">
      <svg width="34" height="34"><use href="#i-video"></use></svg>
      <span class="tag">Vídeo ainda não adicionado</span>
      <p class="caption">3. Editor visual de cenários — EDL-builder</p>
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
    title: '5. Catálogo de modelos e built-in',
    html: `
  <p class="eyebrow">Demonstrações em vídeo</p>
  <h2 class="headline">5. Catálogo de modelos e modelos built-in</h2>
  <div class="demo-stage">
    <video controls playsinline src="demos/07-catalogo-modelos.mp4"></video>
  </div>
`
  },
  {
    extraClass: '',
    title: 'Padronização de ambiente',
    html: `
  <p class="eyebrow">II · A proposta</p>
  <h2 class="headline">Padronização de ambiente</h2>
  <div class="grid-2">
    <ul class="list">
      <li>Extensão VSCode para arquivos <code>.edl</code>: realce de sintaxe — <b>e</b>, mais
      importante, uma validação de verdade desses arquivos.</li>
      <li>A validação roda o <b>mesmo parser</b> que o framework usa em produção, antes mesmo de
      tentar rodar a simulação — o erro de sintaxe aparece no editor, não no meio de um cenário
      carregando.</li>
    </ul>
    <div class="media-placeholder media-placeholder--fill">
      <svg width="30" height="30"><use href="#i-image"></use></svg>
      <span class="tag">Imagem sugerida</span>
      <p class="caption">Screenshot: um erro de slot apontado direto no editor, antes de rodar
      qualquer coisa</p>
    </div>
  </div>
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
    extraClass: 'slide--statement',
    title: 'Reescrever é recuperar conhecimento',
    html: `
  <p class="eyebrow">III · Onde estamos</p>
  <p class="statement">Reescrever modelos que já existem é um desafio grande — e um desafio que
  já sabíamos, desde o início, que teria de ser encarado.</p>
  <p class="lede muted" style="margin-top:4px">O benefício: reescrever é a oportunidade de
  recuperar um conhecimento que hoje só existe dentro do próprio código. E esse é o melhor
  momento possível para isso — com IA tornando muito mais viável ler, entender e reescrever
  código antigo, com documentação e teste junto.</p>
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
        <li>Provas de conceito que mostram até onde essa estrutura pode ir, de forma
        <b>nativa</b>: uma decisão tomada por uma <b>rede neural treinada via ONNX</b>, e uma
        decisão escrita <b>inteiramente em Python</b>, sem recompilar nada.</li>
        <li>Adotando boas práticas de desenvolvimento — não é só "funciona", é rastreável.</li>
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
    <figcaption>Hoje o mesmo ciclo decide em <code>updateTC()</code>, fase 3, até 50 Hz — junto
    com o resto da lógica de decisão da simulação, na mesma cadência da física.</figcaption>
  </figure>
`
  },
  {
    extraClass: '',
    title: 'Paralelismo determinístico',
    html: `
  <p class="eyebrow">III · Onde estamos</p>
  <h2 class="headline">Paralelismo determinístico</h2>
  <p class="lede muted">Cada aeronave decide na própria thread do pool de tempo crítico, em
  paralelo — e o resultado sai <b>byte-idêntico</b> rodando com uma, duas ou quatro threads.</p>
  <figure class="diagram">
    <svg viewBox="0 0 900 260" role="img" aria-label="Quatro aeronaves decidindo cada uma na propria thread do pool de tempo critico, convergindo para um dump byte-identico independente do numero de threads">
      <rect x="20" y="15" width="190" height="70" rx="7" fill="none" stroke="currentColor" stroke-width="1.6" class="svg-teal"/>
      <text x="115" y="47" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="13" font-weight="600" class="svg-teal">falcon1</text>
      <text x="115" y="67" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="10.5" class="svg-teal" opacity=".85">thread do pool</text>

      <rect x="237" y="15" width="190" height="70" rx="7" fill="none" stroke="currentColor" stroke-width="1.6" class="svg-accent"/>
      <text x="332" y="47" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="13" font-weight="600" class="svg-accent">falcon2</text>
      <text x="332" y="67" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="10.5" class="svg-accent" opacity=".85">thread do pool</text>

      <rect x="454" y="15" width="190" height="70" rx="7" fill="none" stroke="currentColor" stroke-width="1.6" class="svg-amber"/>
      <text x="549" y="47" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="13" font-weight="600" class="svg-amber">falcon3</text>
      <text x="549" y="67" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="10.5" class="svg-amber" opacity=".85">thread do pool</text>

      <rect x="671" y="15" width="190" height="70" rx="7" fill="none" stroke="currentColor" stroke-width="1.6" class="svg-brick"/>
      <text x="766" y="47" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="13" font-weight="600" class="svg-brick">falcon4</text>
      <text x="766" y="67" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="10.5" class="svg-brick" opacity=".85">thread do pool</text>

      <line x1="115" y1="85" x2="115" y2="168" stroke="currentColor" stroke-width="1.4" class="svg-ink" opacity=".55" marker-end="url(#arrow)"/>
      <line x1="332" y1="85" x2="332" y2="168" stroke="currentColor" stroke-width="1.4" class="svg-ink" opacity=".55" marker-end="url(#arrow)"/>
      <line x1="549" y1="85" x2="549" y2="168" stroke="currentColor" stroke-width="1.4" class="svg-ink" opacity=".55" marker-end="url(#arrow)"/>
      <line x1="766" y1="85" x2="766" y2="168" stroke="currentColor" stroke-width="1.4" class="svg-ink" opacity=".55" marker-end="url(#arrow)"/>

      <rect x="20" y="170" width="840" height="70" rx="8" fill="none" stroke="currentColor" stroke-width="2" class="svg-ink"/>
      <text x="440" y="202" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="14" font-weight="600" class="svg-ink">dump byte-idêntico</text>
      <text x="440" y="222" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="10.5" class="svg-ink" opacity=".8">com 1, 2 ou 4 threads T/C</text>
    </svg>
    <figcaption>Quatro aeronaves, quatro threads, um único resultado possível — o número de
    threads muda o paralelismo, nunca o resultado.</figcaption>
  </figure>
  <div class="stat-row">
    <span class="stat-chip">1/2/4 threads: dumps byte-idênticos</span>
  </div>
  <div class="callout">
    <span class="k">Onde o ganho aparece</span>
    <p>Maior justamente em <b>simulações isoladas</b> — várias entidades decidindo ao mesmo tempo
    dentro de uma simulação só — e não em lote, rodando várias instâncias em paralelo. É
    paralelismo de dentro de uma simulação, não entre simulações.</p>
  </div>
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
  <div class="callout">
    <span class="k">Antes de pedir ajuda a uma IA</span>
    <p>Reserve um tempo para esse primeiro contato, sem pressa — explorar no seu próprio ritmo
    rende insights mais ricos.</p>
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
  <p class="eyebrow">IV · O convite</p>
  <h2 class="headline">O que se espera</h2>
  <div class="grid-2">
    <ul class="list">
      <li>Que cada pessoa que passar por esse tour deixe esse framework com a <b>cara do que
      gostaria de trabalhar</b> no futuro.</li>
      <li>Chegar a um núcleo o mais próximo possível do <b>imutável</b> — uma base estável o
      suficiente para que o esforço de cada equipe vá para o que é específico.</li>
    </ul>
    <figure class="diagram">
      <svg viewBox="0 0 520 320" role="img" aria-label="Slot, plugin e folha de arvore de comportamento convergindo para um contexto tecnico comum">
        <rect x="20" y="10" width="220" height="60" rx="7" fill="none" stroke="currentColor" stroke-width="1.4" class="svg-teal"/>
        <text x="130" y="47" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="13" class="svg-teal">"slot"</text>

        <rect x="20" y="130" width="220" height="60" rx="7" fill="none" stroke="currentColor" stroke-width="1.4" class="svg-teal"/>
        <text x="130" y="167" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="13" class="svg-teal">"plugin"</text>

        <rect x="20" y="250" width="220" height="60" rx="7" fill="none" stroke="currentColor" stroke-width="1.4" class="svg-teal"/>
        <text x="130" y="280" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="12" class="svg-teal">"folha de árvore</text>
        <text x="130" y="296" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="12" class="svg-teal">de comportamento"</text>

        <rect x="300" y="115" width="200" height="90" rx="8" fill="none" stroke="currentColor" stroke-width="1.7" class="svg-accent"/>
        <text x="400" y="152" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="13" font-weight="600" class="svg-accent">contexto</text>
        <text x="400" y="172" text-anchor="middle" fill="currentColor" font-family="IBM Plex Mono" font-size="13" font-weight="600" class="svg-accent">técnico comum</text>

        <line x1="242" y1="40" x2="298" y2="130" stroke="currentColor" stroke-width="1.3" class="svg-ink" opacity=".55" marker-end="url(#arrow)"/>
        <line x1="242" y1="160" x2="298" y2="160" stroke="currentColor" stroke-width="1.3" class="svg-ink" opacity=".55" marker-end="url(#arrow)"/>
        <line x1="242" y1="280" x2="298" y2="190" stroke="currentColor" stroke-width="1.3" class="svg-ink" opacity=".55" marker-end="url(#arrow)"/>
      </svg>
      <figcaption>Quando os termos significam a mesma coisa para todo mundo, a equipe ganha um
      contexto técnico comum — não um "só entendo meu quadrado".</figcaption>
    </figure>
  </div>
`
  },
  {
    extraClass: 'slide--statement',
    title: 'Fechamento',
    html: `
  <p class="eyebrow">IV · O convite</p>
  <p class="statement">Simulação aeroespacial vai continuar sendo desafiadora. Mas a forma como
  escrevemos, testamos, documentamos e conversamos sobre os modelos que simulam esse domínio —
  <em>essa parte é otimizável</em>.</p>
  <p class="lede muted" style="margin-top:4px">Um eixo de cada vez. Um modelo, hoje. E o convite
  aberto para o próximo.</p>
`
  },
  {
    extraClass: 'slide--statement',
    title: 'Encerramento',
    html: `
  <p class="eyebrow">IV · Encerramento</p>
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
