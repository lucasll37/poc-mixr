# `docs/presentation/demos/` — vídeos do menu de reprodução

O slide "Demonstrações em vídeo" (`docs/presentation/index.html`) lê os arquivos **desta
pasta** pelo nome exato abaixo. Basta soltar o arquivo aqui — nenhuma edição de HTML é
necessária. O player tenta `.mp4`, depois `.webm`, depois `.mov`, nessa ordem; a primeira
extensão que existir é a usada. Enquanto o arquivo não existe, o item mostra um placeholder
("vídeo ainda não adicionado"), nunca um player quebrado ou em branco.

| # | Nome do arquivo (sem extensão) | Conteúdo |
|---|---|---|
| 1 | `01-app` | Funcionamento do `./app`, mostrando as telas possíveis |
| 2 | `02-documentacao-iterativa` | Documentação iterativa |
| 3 | `03-editor-edl` | Editor minimalista de EDL |
| 4 | `04-esteira-build` | Esteira de build: `make clean` → `configure` → `models` → `make build` → `make install` |
| 5 | `05-criacao-modelo` | Criação de modelo com `make` e geração da árvore |
| 6 | `06-groot` | Uso do Groot para acompanhar a simulação |
| 7 | `07-catalogo-modelos` | Catálogo de modelos e modelos built-in |
| 8 | `08-mapa-documentacao` | Mapa de documentação |
| 9 | `09-rl-python` | PoCs de RL e script Python |

Exemplo: para o item 6, salve como `docs/presentation/demos/06-groot.mp4` (ou `.webm`/`.mov`).

## Se a lista mudar

A lista de demos vive em `DEMOS` (array no início do segundo `<script>` de
`docs/presentation/index.html`, logo depois do script de navegação do deck) — cada entrada é
`{ file: 'demos/<nome>', title: '<N>. <título>' }`. Adicionar/remover/renomear um item é editar
essa lista; o menu de reprodução é gerado a partir dela, não precisa tocar em mais nada.
