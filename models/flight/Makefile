.PHONY: all configure build test install clean check-root help
.DEFAULT_GOAL := help

# ==============================================================================
# Makefile AUTOCONTIDO do modelo `flight` (o modelo de producao).
#
# "Autocontido" quer dizer: abra o VS Code AQUI (models/flight/), rode
# `make`, e os dois artefatos (libflight.so / libflight_tc.so) + os dados
# (flight_tree.xml, data/jsbsim/) saem em ./dist -- a raiz DESTE projeto, nao
# a raiz do poc-mixr. Nao chama o Makefile raiz nem o alvo `models` dele.
#
# Pre-requisito, publicado pelo projeto HOST uma vez:
#
#     cd ../.. && make configure && make sdk
#
# Depois disso este Makefile roda sozinho, quantas vezes quiser, so contra o
# que foi publicado em ../../dist e ../../build.
#
# 'make install-host' e o unico alvo que toca o projeto raiz -- mas so ate
# ../../models/plugins/ (lib, flat) e ../../models/plugins/data/flight/
# (arvore + aeronave), o MESMO deposito que um .so de terceiro usaria (ver
# models/plugins/README.md). NUNCA escreve em ../../dist/ diretamente --
# quem sincroniza models/plugins/ -> dist/lib(share)/mixr-plugins/ e o
# 'make install' do projeto raiz (alvo 'sync-plugins'), no HOST, no momento
# em que o HOST de fato instala. Sem rodar 'make install-host' aqui E
# 'make install' na raiz, compilar e util para iterar (ver 'make test') mas
# nao roda em cenario nenhum -- ver models/fixtures/stub/README.md para o
# mesmo "porque" aplicado la.
# ==============================================================================

ROOT      := $(abspath ../..)
PWD       := $(shell pwd)
BUILD_DIR := ./build
DEST_DIR  := $(PWD)/dist
BUILD_TYPE ?= Debug
TESTS     ?= true
VARIANTS  ?= false
ASAN      ?= false

PKG_PATH  := $(ROOT)/dist/lib/pkgconfig,$(ROOT)/build
NINJA_JOBS := $(shell nproc)

RED := \033[0;31m
GREEN := \033[0;32m
NC := \033[0m

all: build ## Alvo padrao: configura e compila (atalho para `build`).

check-root: ## Confere que o projeto HOST ja publicou o SDK (pre-requisito, uma vez).
	@test -f $(ROOT)/build/conan_meson_native.ini || { \
		echo "$(RED)faltando $(ROOT)/build/conan_meson_native.ini$(NC)"; \
		echo "  rode 'cd $(ROOT) && make configure' primeiro (uma vez)."; exit 1; }
	@test -f $(ROOT)/dist/lib/pkgconfig/poc-mixr-sdk.pc || { \
		echo "$(RED)faltando o SDK de plugin em $(ROOT)/dist$(NC)"; \
		echo "  rode 'cd $(ROOT) && make sdk' primeiro (uma vez)."; exit 1; }

configure: check-root ## meson setup, isolado neste projeto (./build), consumindo o SDK/Conan do host por pkg-config.
	@mkdir -p $(BUILD_DIR)
	@# So reconfigura (caro: reavalia TODAS as dependencias, ~1-2s) se o
	@# build.ninja nao existe AINDA, se TESTS/VARIANTS/ASAN/BUILD_TYPE
	@# mudaram desde a ultima vez, OU se meson.build/meson_options.txt
	@# foram editados (STALE) -- 'meson compile'/'install', chamados
	@# depois, ja fazem seu proprio no-op rapido quando nada mudou; era o
	@# --reconfigure INCONDICIONAL (repetido a cada 'make run-*'/'test')
	@# que respondia pela maior parte do tempo de um "so rodar de novo".
	@# A checagem STALE existe porque o NINJA tem seu PROPRIO regen
	@# automatico (dispara sozinho quando meson.build muda, independente
	@# deste guard) e ele NAO repassa '-Dpkg_config_path' -- confirmado
	@# quebrando: editar meson.build e rodar 'make models' de novo, sem
	@# passar por aqui, falhava com "Dependency poc-mixr-sdk not found".
	@# O native-file do Conan entra no STALE junto com os meson.build: um
	@# 'make configure' na RAIZ reescreve conan_meson_native.ini, e o regen
	@# AUTOMATICO do ninja dispara por causa dele -- sem repassar
	@# '-Dpkg_config_path', exatamente como descrito acima. Confirmado
	@# quebrando: 'make configure' na raiz seguido de 'make test' falhava com
	@# "Dependency poc-mixr-sdk not found", com o .pc presente em dist/.
	@#
	@# O proprio poc-mixr-sdk.pc entra pelo MESMO motivo, e a falha e ainda
	@# mais silenciosa: o meson resolve o 'Libs:' do .pc em tempo de CONFIGURE
	@# e grava os caminhos das .so direto no build.ninja. Uma lib NOVA no SDK
	@# (o -lxinfer, por exemplo) nao aparece em lugar nenhum -- nem o guard nem
	@# o regen do ninja percebem -- e o link do plugin falha com "undefined
	@# reference" a uma funcao que existe e esta exportada. Confirmado
	@# quebrando, exatamente assim.
	@#
	@# E por isso que o ramo de reconfiguracao comeca com 'meson configure
	@# --clearcache': o meson CACHEIA o resultado do pkg-config entre
	@# execucoes, e '--reconfigure' sozinho reusa o cache -- medido, a
	@# dependencia continuava com as 4 .so antigas do SDK depois de um
	@# reconfigure completo. ('--clearcache' e argumento de 'meson configure',
	@# nao de 'meson setup' -- passa-lo ao setup faz o meson recusar a linha
	@# inteira, e a falha se parece com "nao mudou nada".)
	@WANT="$(BUILD_TYPE)|$(TESTS)|$(VARIANTS)|$(ASAN)"; \
	 GOT=$$(cat $(BUILD_DIR)/.configure-args 2>/dev/null || echo ""); \
	 STALE=$$(find meson.build meson_options.txt $(ROOT)/build/conan_meson_native.ini $(ROOT)/dist/lib/pkgconfig/poc-mixr-sdk.pc -newer $(BUILD_DIR)/build.ninja 2>/dev/null); \
	 if [ -f $(BUILD_DIR)/build.ninja ] && [ "$$WANT" = "$$GOT" ] && [ -z "$$STALE" ]; then \
	    :; \
	 else \
	    if [ -f $(BUILD_DIR)/build.ninja ]; then \
	       meson configure $(BUILD_DIR) --clearcache >/dev/null 2>&1 || true; \
	    fi; \
	    meson setup --reconfigure \
	       --backend ninja \
	       --buildtype $(shell echo $(BUILD_TYPE) | tr '[:upper:]' '[:lower:]') \
	       --native-file $(ROOT)/build/conan_meson_native.ini \
	       --prefix=$(DEST_DIR) \
	       --libdir=$(DEST_DIR)/lib \
	       -Dpkg_config_path=$(PKG_PATH) \
	       -Dtests=$(TESTS) \
	       -Dvariants=$(VARIANTS) \
	       -Dasan=$(ASAN) \
	       $(BUILD_DIR)/ .; \
	    echo "$$WANT" > $(BUILD_DIR)/.configure-args; \
	 fi

build: configure ## Compila libflight.so + libflight_tc.so.
	meson compile -C $(BUILD_DIR) -j$(NINJA_JOBS)

install: build ## Instala em ./dist (a raiz DESTE projeto): lib/mixr-plugins/*.so + share/mixr-plugins/flight/ (arvore + aeronave).
	@# '--only-changed': sem isto, 'meson install' recopia e atualiza o mtime
	@# do destino MESMO com conteudo identico -- e um destino mais novo que
	@# um INPUT ja linkado (ex.: dist/lib/libxtrack.so, do SDK, do qual
	@# libflight.so depende) faz o ninja achar que precisa RELINKAR na
	@# proxima chamada, mesmo sem nada ter mudado de verdade -- confirmado
	@# com 'ninja -C build -d explain libflight.so': "output libflight.so
	@# older than most recent input .../libxtrack.so". Sem isto, 'make
	@# run-*'/'make test' relinkavam os quatro plugins TODA vez, mesmo com
	@# tudo ja compilado.
	meson install -C $(BUILD_DIR) --no-rebuild --only-changed
	@for so in libflight.so libflight_tc.so; do \
		ldd $(DEST_DIR)/lib/mixr-plugins/$$so | grep -q 'not found' && \
			{ echo "$(RED)install: $$so com dependencia nao resolvida$(NC)"; exit 1; } || true; \
	done
	@echo "$(GREEN)install: OK$(NC) -> $(DEST_DIR)/lib/mixr-plugins/, $(DEST_DIR)/share/mixr-plugins/flight/"

test: build ## Roda a suite do modelo (domain + tree + native).
	@N=$$(meson introspect --tests $(BUILD_DIR) | python3 -c 'import json,sys; print(len(json.load(sys.stdin)))'); \
	 [ "$$N" -ge 3 ] || { echo "$(RED)suite incompleta ($$N de 3) -- confira TESTS=true$(NC)"; exit 1; }
	meson test -C $(BUILD_DIR) --print-errorlogs

install-host: install ## Deposita em ../../models/plugins/ (lib, flat) e .../data/flight/ (dados) -- NAO toca dist/.
	@mkdir -p $(ROOT)/models/plugins $(ROOT)/models/plugins/data/flight
	@cp -a $(DEST_DIR)/lib/mixr-plugins/. $(ROOT)/models/plugins/
	@cp -a $(DEST_DIR)/share/mixr-plugins/flight/. $(ROOT)/models/plugins/data/flight/
	@echo "$(GREEN)install-host: OK$(NC) -> $(ROOT)/models/plugins/, $(ROOT)/models/plugins/data/flight/"
	@echo "  (dist/ so e populado por 'cd $(ROOT) && make install')"

clean: ## Remove ./build e ./dist (LOCAIS -- nao mexe no ../../dist do host).
	rm -rf $(BUILD_DIR) $(DEST_DIR)

help:
	@grep -E '^[a-zA-Z0-9_-]+:.*?## .*$$' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-16s\033[0m %s\n", $$1, $$2}'
