# `docs/presentation/demos/` — imagens e vídeos dos slides

Todo slot de mídia do deck é procurado **por nome de arquivo nesta pasta**. Soltar o arquivo
aqui com o nome da tabela abaixo é tudo o que é preciso — **nenhuma edição de HTML/JS, nenhum
passo de build**. Enquanto o arquivo não existir, o slide mostra um placeholder tracejado que
anuncia, ele mesmo, o nome esperado.

**A extensão não está fixada.** Cada slot tenta, nesta ordem, o tipo esperado e depois o outro:
vídeo `.mp4` → `.webm` → `.mov`, imagem `.png` → `.jpg` → `.jpeg` → `.webp` → `.gif` → `.svg`.
A coluna "tipo" abaixo é só a sugestão do roteiro (e a ordem de sondagem) — **trocar um vídeo
por um print, ou o contrário, em qualquer slot, não exige tocar em código nenhum**.

| # | Slide | Tipo sugerido | Nome do arquivo (sem extensão) | Conteúdo |
|---|---|---|---|---|
| 08 | Um repositório, não vários | imagem | `repo-org` | O repositório integrado — um histórico de git, uma árvore de pastas, sem asa-models/asa-libs/asa-models-r espalhados |
| 10 | Um contrato forte para todo modelo | imagem | `modelo-estrutura` | Pasta de um modelo real — `tests/`, `docs/`, `README.md`, `CHANGELOG.md`, `Makefile` |
| 12 | 1. `./app` — painel de controle | vídeo | `01-app` | Funcionamento do `./app`, mostrando as telas possíveis |
| 13 | 2. Documentação iterativa | vídeo | `02-documentacao-iterativa` | O manual interativo |
| 14 | 3. Editor visual de cenários | vídeo | `03-editor-edl` | O EDL-builder montando um cenário |
| 15 | 4. Code highlight para `.edl` | imagem | `04-edl-highlight` | Um erro de slot apontado direto no editor, antes de rodar qualquer coisa |
| 18 | O primeiro entregável | imagem | `tacview-a4` | Tacview mostrando a A-4 Skyhawk em voo, com os falcons e o intruso |
| 19 | A árvore de comportamento é um contrato | vídeo | `arvore-comportamento` | A mesma árvore com folha C++, depois Python, depois política ONNX — sem recompilar |
| 23 | Comece pelo TOUR.md | vídeo | `tour` | Walkthrough: subir o ambiente, rodar um cenário, ver no Tacview |
| 24 | Estresse o metaprojeto | imagem | `livros-capas` | Capas dos dois livros — manual do MIXR e do BehaviorTree.CPP |
| 27 | Encerramento | imagem | `encerramento` | Logo da ASA ou imagem de encerramento do time |

A coluna `#` é a posição do slide no deck (a mesma que o HUD mostra), não uma numeração de
arquivo — só os quatro slides da seção "Demonstrações" carregam número no próprio nome do
arquivo, herdado do roteiro.

Exemplo: para o slot 14, salve como `docs/presentation/demos/03-editor-edl.mp4` (ou `.webm`/
`.mov`); para o 18, `tacview-a4.png` (ou `.jpg`/...).

## Arquivos órfãos

Qualquer outro arquivo aqui é ignorado — fica em disco sem efeito nenhum na apresentação. Hoje
é o caso de `04-esteira-build.mp4`, `07-catalogo-modelos.mp4`, `app.mp4`, `pipeline.mp4`,
`Behavior.mp4`, `Single-thread.mp4`, `Multi-thread.mp4`, `2026-09-08 22-24-24.mp4` e
`Screenshot from 2026-09-08 17-11-33.png`: sobraram de uma numeração anterior (com mais slides
de demonstração) ou de gravações ainda sem slot no roteiro atual (`SLIDE.md`). Para aproveitar
um deles, basta renomeá-lo para o nome da tabela acima — ex.: `Behavior.mp4` →
`arvore-comportamento.mp4`.

## Se a lista mudar

Cada slot é um `data-media="demos/<nome>"` (+ `data-media-kind="image|video"`) num item do array
`window.PRESENTATION_SLIDES`, em `docs/presentation/content.js` — nos slides de demonstração é um
`.demo-stage` (vídeo em tela cheia); no meio do conteúdo, um `.media-placeholder`. O terceiro
`<script>` de `docs/presentation/index.html` faz a sondagem para os dois. Adicionar, remover ou
renomear um slot é editar essa entrada em `content.js`; o deck inteiro (contagem de slides, HUD,
mapa de navegação) é gerado a partir dele.
