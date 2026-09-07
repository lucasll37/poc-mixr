# single-thread/multi-thread/bandit moram em ./poc/ (as tres pocs de
# execucao real -- ver src/poc/meson.build). O dashboard (TUI de
# controle/monitoramento) NAO mora aqui -- ver ./app/meson.build,
# subdir'ado direto da raiz. E o UNICO ocupante de ./app/, por isso o alvo
# se chama 'app', nao 'dashboard'.
subdir('./poc')

# O wrapper Gymnasium -- peer de ./poc/, tambem nao mais uma poc:
# modulo de extensao Python + pacote Python puro, nem sequer produz um
# executavel. Ver src/rl/README.md.
subdir('./rl')

# ./ui/ (o editor grafico de cenario .edl) NAO tem subdir() aqui, de proposito:
# e so JSX/JS/JSON + scripts Node/Python proprios (src/ui/scripts/), a mesma
# categoria de "mora sob src/ mas nao e Meson" que src/poc/rl-training/ ja e --
# ver o comentario daquele em src/poc/meson.build. `make open-edl-builder`
# (Makefile raiz, unico alvo make deste editor) e quem builda, sem passar
# pelo grafo do host.
