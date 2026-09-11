# `src/ui` — editor gráfico de cenário `.edl`

Um **`.edl`** — **EDL** (*English Description Language*), a linguagem de configuração nativa do
MIXR — é o arquivo de texto declarativo que descreve um cenário (*players*, sensores, rede, taxas)
sem recompilar nada (ver glossário no [`README.md`](../../README.md) raiz). Esta é uma ferramenta
gráfica (React, no navegador, sem servidor) para montar um cenário `.edl` do zero ou
**carregar um `.edl`/`.edl.in` REAL já existente**: arrastar classes de uma paleta completa (todas
as factories do host, `libs/x*` e os plugins deste repositório), preencher campos e exportar `.edl`
válido — sem ler C++ nem decorar a gramática do `edl_parser`.

## Como se usar

```bash
make open-edl   # gera tudo do zero e abre no navegador -- unico alvo make deste editor
```

**Pré-requisito** (já listado na tabela de Pré-requisitos do `README.md` raiz, linha "Node.js +
npm"): `src/ui/scripts/compile.js` baixa React/ReactDOM via `curl` de `cdnjs.cloudflare.com` e
instala `@babel/standalone` via `npm` (cache local em `src/ui/.cache/`, só na primeira execução)
— este alvo precisa de **Node.js + npm** instalados e de rede liberada para
`cdnjs.cloudflare.com`/`registry.npmjs.org`. Sem isso, falha na hora.

A árvore nasce vazia, só com a raiz (`Station`/`ClockStation`). Clique num nó para selecioná-lo;
arraste uma classe da paleta esquerda para um slot para adicionar um filho — só classes EM
CONFORMIDADE com o que aquele slot de fato aceita aparecem como opção (um slot que só aceita
texto, como `TacviewOutput.typeMap`, nunca sugere classe nenhuma — só o botão "+ texto").
"Expandir tudo"/"Recolher tudo" abrem ou fecham todo nó de uma vez, útil no preset de 53
componentes. Três abas ao lado da árvore: **Mapa** (posição de cada `Player`), **Abertos** (o que
ainda falta resolver antes de exportar — hoje, placeholder de template `@TOKEN@` ainda literal —,
com contagem sempre visível no rótulo da aba; clique num item para pular direto ao nó, e o número
cai NA HORA quando o campo é corrigido) e a **prévia `.edl`** (sempre visível, ao LADO das outras
duas — não embaixo). `.eb-main-split` põe TRÊS colunas irmãs lado a lado, cada uma com a própria
barra de rolagem: a aba ativa (árvore/mapa/abertos), o **painel de propriedades** do nó
selecionado (editar campos), e a prévia `.edl`, NESSA ORDEM — de propósito: selecionar um nó
mostra os campos dele bem ao lado, e a prévia, mais à direita ainda, já chega com a região
correspondente **destacada** (fundo âmbar sobre o texto, com scroll automático até lá) — ver
"Destaque na prévia .edl", abaixo. A paleta à esquerda é colapsável (botão «/» no topo dela, ou no
canto quando colapsada) — devolve a largura pro trio árvore/propriedades/prévia quando o catálogo
não é o que importa no momento; a preferência fica salva entre sessões. O botão "Carregar preset"
troca a árvore vazia pelo cenário de mais componentes do repositório (`built-in_mixr_1`), como
ponto de partida.

"Exportar .edl" grava **direto** em `sandbox/<nome>/configs/scenario.edl`, quando o navegador
permite. Em Chrome/Edge (File System Access API — `"showDirectoryPicker" in window`), o botão
"Escolher pasta sandbox/…" ao lado do campo de nome pede permissão UMA vez por sessão pra escrever
na pasta `sandbox/` do repositório; feito isso, o botão principal vira "Salvar em sandbox/" e
grava o arquivo sem download nenhum — trocar o nome do cenário só muda a SUBPASTA
(`sandbox/<slug>/configs/`) que é criada/sobrescrita, sem pedir a pasta de novo a cada exportação.
Sem a API (Firefox), ou antes de escolher a pasta, cai no download de sempre (sugerindo
`sandbox/<nome>/configs/scenario.edl` como nome — Chrome/Firefox criam a subpasta dentro de
Downloads/). Se a escrita direta falhar no meio do caminho (permissão expirada — o handle não
sobrevive a um F5) ela cai pro download automaticamente, com um aviso explicando o motivo. Se
nem o download disparar, há um segundo fallback (abre o conteúdo numa aba nova, pra salvar
manualmente) e um aviso apontando pra copiar o texto direto da prévia como último recurso.

### Prévia `.edl`: numeração de linha e destaque de seleção

A prévia é uma lista de linhas com **numeração à esquerda, estilo IDE** (coluna fixa, sticky ao
rolar para o lado numa linha comprida) — não é mais um bloco de texto corrido.

Selecionar um nó (clique na árvore, ou no marcador de um `Player` na aba Mapa — qualquer caminho
que mude a seleção) acende, na prévia, todas as linhas que o trecho `( Fábrica ... ) // Fábrica`
daquele nó ocupa (e de tudo que está aninhado dentro dele — selecionar um container grande destaca
o bloco inteiro que ele ocupa, o que é esperado: é literalmente a região dele no arquivo) e rola a
prévia até lá. O destaque é um **retângulo por linha** — toda linha tocada acende POR INTEIRO, até
o fim dela, mesmo que o trecho do nó só comece ou termine no meio dela (ex.: a linha
`dynamicsModel: ( JSBSimModel` acende inteira, não só a partir do `(`) — não é um sublinhado que
"abraça" só os caracteres exatos do nó. Editar um campo do nó selecionado também rola a prévia de
novo, automaticamente, pra a edição continuar visível mesmo que o texto tenha crescido/encolhido e
empurrado a região destacada pra fora da parte visível.

A posição vem de `projectToEdlWithSpans()` (`edl_builder_core.js`), a mesma lógica de serialização
de sempre (`projectToEdl`, que continua com o contrato/saída idênticos), só que também devolvendo
onde cada nó caiu no texto final — não é busca de texto nem heurística, é calculado durante a
própria montagem do `.edl`. `splitTokensIntoLines()`/`computeLineRanges()` (mesmo arquivo) reagrupam
isso por linha pra alimentar a numeração e decidir, linha a linha, se ela entra no retângulo.

**O destaque é GRANULAR por campo enquanto se edita.** Clicar num campo já preenchido no painel de
propriedades (não só selecionar o nó na árvore) estreita o destaque pra só a linha daquele campo
(`nome: valor`), em vez do bloco inteiro do nó — útil num nó com muitos slots, onde o bloco inteiro
podia ter dezenas de linhas. Sair do campo (trocar de nó, ou o campo nunca ter sido preenchido)
volta pro destaque do nó inteiro. A posição granular também vem de `projectToEdlWithSpans()` — cada
linha de slot-folha ganha uma chave própria no mapa de spans (`"<id-do-nó>#<slot>"`), além da chave
do nó em si.

### Extensão do arquivo: `.edl` ou `.edl.in`

"Exportar .edl" e "Meus presets" (abaixo) escolhem a extensão sozinhos, olhando a aba **Abertos**:
com QUALQUER placeholder de template (`@TOKEN@`) ainda pendente, o arquivo baixa como `.edl.in`
— um arquivo assim não é um `.edl` válido pro parser real do MIXR (que rejeitaria o `@TOKEN@`
como token desconhecido), é um TEMPLATE ainda por preencher, e a extensão precisa dizer isso —
mesma convenção já usada em todo o repositório (`scenario.edl.in` vira `scenario.edl` só depois de
resolvido). Sem nenhum placeholder pendente, é `.edl` normal. O botão já mostra a extensão que vai
ser usada ("Exportar .edl" ou "Exportar .edl.in").

### "Meus presets" — salvar um `.edl`/`.edl.in` próprio

Logo abaixo da prévia: a seção **"Meus presets"** salva a árvore atual como
`preset_<nome>.edl`/`.edl.in` — `<nome>` vem do campo ao lado, ou do nome do cenário já digitado
acima, ou `base`. Mesmo bloqueio de ASCII de "Exportar .edl". Não confundir com o botão
"Carregar preset" da barra, no topo: aquele carrega o ÚNICO cenário de exemplo embutido no build;
aqui são quantos `.edl` você quiser salvar, com nome próprio.

Tem o PRÓPRIO botão "Escolher pasta…" (handle de diretório separado do de "Exportar .edl" acima —
são pastas diferentes: `sandbox/<nome>/configs/` vs. onde quer que você guarde presets, sugerido
`src/ui/presets/` mas não presumido). Com uma pasta escolhida, "Salvar na pasta escolhida" grava
direto, sem download; sem isso (ou em Firefox, que não tem a API), "Salvar como preset" baixa o
arquivo do jeito de sempre.

A seção nasce **colapsada** (só o botão "▸ Meus presets"), pra maximizar a altura da prévia `.edl`
logo acima dela — a preferência de aberta/fechada fica salva entre sessões. Se nem a escrita direta
nem o download dispararem por nenhum caminho (raro, mas relatado), um aviso na tela aponta pra
selecionar o texto na prévia e copiar manualmente.

Não há mais um "Carregar"/"Apagar" dedicado a preset — um preset salvo é só mais um `.edl`/`.edl.in`
como outro qualquer: reabra com o botão **"Carregar .edl"**, de onde quer que você tenha guardado o
arquivo. `src/ui/presets/` é só uma convenção de PASTA sugerida pra organizar os seus (ver
`src/ui/presets/README.md`) — não versionada, e a ferramenta não escreve nela sozinha.

### "Carregar .edl" — abrir um cenário real

O botão "Carregar .edl" lê um `.edl`/`.edl.in` do disco e mapeia TUDO para a mesma árvore de
cartões que a ferramenta já edita — pronto pra mexer e reexportar. Fábrica ou slot que o catálogo
gerado ainda não conhece (classe em desenvolvimento, plugin de terceiro nunca introspectado, erro
de digitação) **nunca é descartado**: fica preservado byte-fiel como valor bruto (marcado com um
`?` tracejado, editável no painel de propriedades como texto — "slots não catalogados") em vez de
sumir silenciosamente numa próxima exportação. Depois de carregar, uma faixa dispensável lista os
avisos não-bloqueantes da CARGA (fábrica/slot não catalogado, ASCII fora do padrão, conteúdo
depois da primeira forma raiz); um clique num aviso pula direto pro cartão correspondente, e um
contador "N não catalogados" continua visível na barra de abas mesmo depois de dispensar a faixa.
Placeholder `@TOKEN@` **não** entra nessa faixa: ele é fato sobre a árvore VIVA, não sobre a
carga, e por isso mora na aba **Abertos**, que recalcula a cada edição em vez de congelar no
instante em que o arquivo foi aberto.

Dois limites explícitos:

- **`@include:frag@` não é suportado por este botão** — o identificador nu do scanner real inclui
  `@` e para no `:`, então deixar sem expandir corromperia a estrutura em silêncio (viraria um slot
  fantasma), não só falharia; por isso é sempre um erro claro, nomeando o fragmento. Nenhum `.edl`
  deste repositório usa isso hoje; se algum dia precisar, substitua a diretiva pelo conteúdo do
  fragmento direto no arquivo antes de carregar.
- **`@TOKEN@` (fora de `@include:`) carrega literal**, sem adivinhar um valor — a aba **Abertos**
  aponta cada ocorrência; edite o campo com o valor real antes de exportar, como qualquer outro
  campo.

### Não há mais "Salvar projeto"/"Abrir projeto" (formato JSON próprio)

Existia um formato de projeto JSON PRÓPRIO desta ferramenta (a mesma árvore `{id,factory,
slotValues,children}` serializada crua), usado só pra salvar/reabrir um trabalho em andamento
antes de "Carregar .edl" existir. Removido — era um formato paralelo ao `.edl` de verdade, e agora
que a ferramenta abre `.edl`/`.edl.in` reais diretamente, mantê-lo só duplicaria o caminho de
"salvar trabalho em andamento" sem necessidade: o próprio `.edl`/`.edl.in` exportado já É o
formato de troca — reabra-o com "Carregar .edl" a qualquer momento pra continuar editando, sem
precisar de um JSON intermediário que nenhuma outra ferramenta lê.

Validar o `.edl` exportado, fora da própria ferramenta:

```bash
python3 src/ui/scripts/edl_lint.py <arquivo>   # leve -- fabrica/slot desconhecido, ASCII, etc.
dist/bin/edlcheck <arquivo>                    # o parser MIXR de verdade, apos 'make install'
```

## Leia mais

[CLAUDE.md](../../CLAUDE.md), seção `src/ui` — toda decisão de design e armadilha confirmada
rodando.
