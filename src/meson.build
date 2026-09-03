# single-thread/multi-thread/bandit-dis moram em ./poc/ (as tres pocs de
# execucao real -- ver src/poc/meson.build). O dashboard (TUI de
# controle/monitoramento) NAO mora aqui -- ver ./app/meson.build,
# subdir'ado direto da raiz. E o UNICO ocupante de ./app/, por isso o alvo
# se chama 'app', nao 'dashboard'.
subdir('./poc')

# API REST de simulacao -- peer de ./poc/, nao mais uma poc: recebe um
# cenario por HTTP, roda num sim-runner isolado (um processo por
# requisicao) e devolve telemetria em JSON. Ver src/server/README.md.
subdir('./server')

# O wrapper Gymnasium -- peer de ./poc/ e ./server/, tambem nao mais uma poc:
# modulo de extensao Python + pacote Python puro, nem sequer produz um
# executavel. Ver src/rl/README.md.
subdir('./rl')
