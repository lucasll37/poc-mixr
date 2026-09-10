# `docs/presentation/demos/` — vídeos dos slides de demonstração

Cada slide `slide--demo` de `docs/presentation/content.js` referencia um arquivo **desta
pasta** pelo nome exato abaixo. Basta soltar o arquivo aqui — nenhuma edição de HTML é
necessária. Um slide com arquivo já existente aponta direto pra ele (`<video src="demos/...">`);
o slide do editor EDL (item 3) ainda não tem arquivo, então usa o mecanismo de sondagem — tenta
`.mp4`, depois `.webm`, depois `.mov`, nessa ordem, e cai no placeholder ("vídeo ainda não
adicionado") enquanto nenhum dos três existir.

| # | Nome do arquivo (sem extensão) | Conteúdo |
|---|---|---|
| 1 | `01-app` | Funcionamento do `./app`, mostrando as telas possíveis |
| 2 | `02-documentacao-iterativa` | Documentação iterativa — o manual interativo |
| 3 | `03-editor-edl` | Editor visual de cenários — EDL-builder (ainda sem arquivo) |
| 4 | `04-esteira-build` | Esteira de build: `make clean` → `configure` → `models` → `make build` → `make install` |
| 5 | `07-catalogo-modelos` | Catálogo de modelos e modelos built-in |

Exemplo: para o item 3, salve como `docs/presentation/demos/03-editor-edl.mp4` (ou `.webm`/`.mov`).

Os arquivos `05-criacao-modelo`, `06-groot`, `08-mapa-documentacao` e `09-rl-python` (de uma
numeração anterior, com mais slides de demo) não são mais referenciados por nenhum slide — o
roteiro atual (`docs/presentation/SLIDE.md`) não pede vídeo dedicado para esses tópicos. Se
algum deles existir aqui, fica apenas como arquivo órfão, sem efeito na apresentação.

## Se a lista mudar

Cada slide de demonstração é uma entrada do array `window.PRESENTATION_SLIDES`, em
`docs/presentation/content.js`, com `extraClass: 'slide--demo'` — um vídeo já existente é
`<video controls playsinline src="demos/<nome>.mp4">`; um ainda sem arquivo usa
`<div class="demo-stage" data-demo="demos/<nome>">` (o terceiro `<script>` de
`docs/presentation/index.html` faz a sondagem `.mp4`/`.webm`/`.mov` para esse padrão).
Adicionar/remover/renomear um item de demonstração é editar essa entrada em `content.js` — o
deck inteiro (contagem de slides, HUD, mapa de navegação) é gerado a partir dele, não precisa
tocar em mais nada.
