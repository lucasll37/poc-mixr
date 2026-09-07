# `src/ui` — editor gráfico de cenário `.edl`

Ferramenta gráfica (React, no navegador, sem servidor) para montar um cenário `.edl` do zero ou
**carregar um `.edl`/`.edl.in` REAL já existente**: arrastar classes de uma paleta completa (todas
as factories do host, `libs/x*` e os plugins deste repositório), preencher campos e exportar `.edl`
válido — sem ler C++ nem decorar a gramática do `edl_parser`.

## Como se usar

```bash
make open-edl-builder   # gera tudo do zero e abre no navegador -- unico alvo make deste editor
```

A árvore nasce vazia, só com a raiz (`Station`/`ClockStation`). Clique num nó para selecioná-lo
(o painel à direita edita os campos dele); arraste uma classe da paleta esquerda para um slot para
adicionar um filho — só classes EM CONFORMIDADE com o que aquele slot de fato aceita aparecem como
opção (um slot que só aceita texto, como `TacviewOutput.typeMap`, nunca sugere classe nenhuma —
só o botão "+ texto"). "Expandir tudo"/"Recolher tudo" abrem ou fecham todo nó de uma vez, útil no
preset de 53 componentes. Três abas ao lado da árvore: **Mapa** (posição de cada `Player`),
**Pendências** (todo papel/slot esperado ainda vazio, com contagem sempre visível no rótulo da
aba — clique num item para pular direto ao nó) e a **prévia `.edl`** (sempre visível, embaixo das
outras duas). O botão "Carregar preset" troca a árvore vazia pelo cenário de mais componentes do
repositório (`built-in_mixr_1`), como ponto de partida. "Exportar .edl" baixa o arquivo; em
Chrome/Edge, "Escolher pasta sandbox/…" aponta a ferramenta pra pasta `sandbox/` deste
repositório e o botão vira "Salvar em sandbox/", escrevendo direto em
`sandbox/<nome>/configs/scenario.edl` — sem passo manual de mover o arquivo baixado.

### "Carregar .edl" — abrir um cenário real

O botão "Carregar .edl" lê um `.edl`/`.edl.in` do disco e mapeia TUDO para a mesma árvore de
cartões que a ferramenta já edita — pronto pra mexer e reexportar. Fábrica ou slot que o catálogo
gerado ainda não conhece (classe em desenvolvimento, plugin de terceiro nunca introspectado, erro
de digitação) **nunca é descartado**: fica preservado byte-fiel como valor bruto (marcado com um
`?` tracejado, editável no painel de propriedades como texto — "slots não catalogados") em vez de
sumir silenciosamente numa próxima exportação. Depois de carregar, uma faixa dispensável lista os
avisos não-bloqueantes (fábrica/slot não catalogado, ASCII fora do padrão, placeholder de template
ainda literal); um clique num aviso pula direto pro cartão correspondente, e um contador "N não
catalogados" continua visível na barra de abas mesmo depois de dispensar a faixa.

Dois limites explícitos:

- **`@include:frag@` não é suportado por este botão** — o identificador nu do scanner real inclui
  `@` e para no `:`, então deixar sem expandir corromperia a estrutura em silêncio (viraria um slot
  fantasma), não só falharia; por isso é sempre um erro claro, nomeando o fragmento. Nenhum `.edl`
  deste repositório usa isso hoje; se algum dia precisar, substitua a diretiva pelo conteúdo do
  fragmento direto no arquivo antes de carregar.
- **`@TOKEN@` (fora de `@include:`) carrega literal**, sem adivinhar um valor — um aviso aponta
  cada ocorrência; edite o campo com o valor real antes de exportar, como qualquer outro campo.

### Não há mais "Salvar projeto"/"Abrir projeto" (formato JSON próprio)

Existia um formato de projeto JSON PRÓPRIO desta ferramenta (a mesma árvore `{id,factory,
slotValues,children}` serializada crua), usado só pra salvar/reabrir um trabalho em andamento
antes de "Carregar .edl" existir. Removido — era um formato paralelo ao `.edl` de verdade, e agora
que a ferramenta abre `.edl`/`.edl.in` reais diretamente, mantê-lo só duplicaria o caminho de
"salvar trabalho em andamento" sem necessidade: o próprio `.edl` exportado (via "Exportar .edl"/
"Salvar em sandbox/") já É o formato de troca — reabra-o com "Carregar .edl" a qualquer momento
pra continuar editando, sem precisar de um JSON intermediário que nenhuma outra ferramenta lê.

Validar o `.edl` exportado, fora da própria ferramenta:

```bash
python3 src/ui/scripts/edl_lint.py <arquivo>   # leve -- fabrica/slot desconhecido, ASCII, etc.
dist/bin/edlcheck <arquivo>                    # o parser MIXR de verdade, apos 'make install'
```

## Leia mais

[CLAUDE.md](../../CLAUDE.md), seção `src/ui` — toda decisão de design e armadilha confirmada
rodando.
