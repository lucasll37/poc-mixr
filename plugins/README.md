# `plugins/` — depósito flat de `.so` (próprio ou de terceiro)

Esta pasta **não é um projeto** — não tem `meson.build`, não é compilada, não é descoberta pela
guarda `tests/guard/check_modelo_estrutura.sh` (que só varre `models/` por `project()`). É um
**depósito**: um lugar em disco onde um `.so` de plugin (mais os dados que ele precisa) pousa
antes de ficar visível para um cenário.

Mora na **raiz** do repositório, ao lado de `build/`/`dist/`, e não dentro de `models/` — de
propósito: nunca foi um projeto Meson, então nunca fez sentido aninhado na pasta que guarda o
FONTE dos modelos. É puramente um artefato binário, como `dist/`.

## Os dois jeitos de algo chegar aqui

1. **Compilado por este repositório** — `make models` (na raiz) chama `install-host` de cada
   projeto de modelo (`models/players/A-4/`, `models/template/`, este último instalado
   incondicionalmente à parte por causa do mirror de contrato — ver o Makefile raiz), e cada um
   copia o próprio `.so` (e dados, se houver) para cá. Nenhum deles escreve em `dist/`
   diretamente — ver a seção "Desacoplando `models` de `dist/`" no `CLAUDE.md` da raiz para o
   "porquê".
2. **Um `.so` de terceiro** — já compilado fora deste repositório (por outra equipe, outro
   fornecedor), entra aqui do mesmo jeito: solto nesta pasta, com o nome que ele já tem.

**A partir do momento em que os dois estão aqui, são indistinguíveis.** `make install` (alvo
`sync-plugins`) copia `plugins/*.so` para `dist/lib/mixr-plugins/` e `plugins/data/` para
`dist/share/mixr-plugins/` — o mesmo passo de cópia cobre os dois casos, sem saber (nem precisar
saber) qual `.so` veio de onde. É o que faz "um cenário de produção rodando com um `.so` que
chegou pelo depósito de terceiro" (`tests/plugin/run_thirdparty_deposit.py`) uma prova real, não
simulada.

## O que pousa aqui, e o que não

- **Flat**: os `.so` vão direto na raiz desta pasta, sem subpasta por modelo.
- **`data/`** é a única exceção ao depósito flat — hoje só `data/flight/` (a árvore de
  comportamento + a aeronave JSBSim, publicadas por `models/players/A-4/`, porque são dado do
  MODELO, não do cenário).
- **`.so` de terceiro não é versionado** (`.gitignore`: `plugins/*.so`, `plugins/data/`) — é
  binário, e no caso de terceiro nem é nosso para versionar. `.gitkeep`/este `README.md` são o
  que sobrevive num clone limpo.
- **`make clean` só remove o que ESTE repositório publicou, por NOME** — nunca por glob nem por
  lista escrita à mão. Os nomes saem do `./dist` local de cada modelo (é dele que o
  `uninstall-host` de cada um deriva o que remover, sem literal cravado no Makefile), então um
  `.so` de terceiro com outro nome, ou um `data/<subpasta>` de terceiro, nunca é apagado por
  engano. O `data/` só é recolhido se tiver ficado **vazio** (`rmdir`, que falha sozinho e sem
  estrago se houver qualquer coisa dentro).

  > **Correção registrada (não redescobrir):** esta promessa já foi **falsa** na metade `data/`.
  > O alvo `clean` tinha um `rm -rf plugins/data` **incondicional** — o namespace inteiro, sem
  > filtro de nome —, então o dado de um terceiro ia junto. A metade `.so` sempre foi verdadeira;
  > a de `data/` passou a ser em conjunto com o alvo `rm-model`.

- **Remover um modelo tem comando próprio: `make rm-model`** (`scripts/models.sh --remove`). Ele
  descobre os artefatos **antes** de apagar a pasta — a informação de quem publicou o quê mora
  dentro dela e não é recuperável depois — e **recusa** se algum cenário `.edl`/`.edl.in` ainda
  apontar para o `.so`. Para um `.so` que sobrou de um modelo já apagado à mão, o modo por
  artefato: `make rm-model SO=libX.so [DATA=dir]`.

- **`make install` avisa sobre `.so` não reivindicado.** Todo sync imprime um aviso amarelo (nunca
  erro) para cada `.so` daqui que nenhum modelo de `models/` reclama. É esperado e correto para um
  `.so` de terceiro — é justamente por eles serem indistinguíveis que isto é aviso e não falha.

## Pasta vazia não é erro

Um clone limpo, ou um `make clean`, deixa esta pasta sem nenhum `.so` — `make models`/`make
install` tratam isso como estado normal (a pasta simplesmente ainda não tem nada depositado),
não como falha.

## Ler também

- [`../CLAUDE.md`](../CLAUDE.md), seção "Desacoplando `models` de `dist/`" — o "porquê" completo
  da separação entre este depósito e `dist/`, e a seção "O MODELO é um plugin, construído numa
  etapa PRÉVIA" para a estrutura de `models/` e o build em etapas
- [`../tests/plugin/run_thirdparty_deposit.py`](../tests/plugin/run_thirdparty_deposit.py) — a
  prova de que um `.so` depositado aqui roda a simulação de produção de verdade
