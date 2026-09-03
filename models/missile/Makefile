.PHONY: all configure build test install clean check-root help
.DEFAULT_GOAL := help

# ==============================================================================
# Makefile AUTOCONTIDO do modelo `missile`.
#
# "Autocontido" quer dizer: abra o VS Code AQUI (models/missile/), rode
# `make`, e o .so sai em ./dist -- a raiz DESTE projeto, nao a raiz do
# poc-mixr. Nao chama o Makefile raiz nem o alvo `models` dele.
#
# Pre-requisito, publicado pelo projeto HOST uma vez:
#
#     cd ../.. && make configure && make sdk
#
# `install-host` e o unico alvo que escreve fora de ./dist -- mas so ate
# ../../models/plugins/, o deposito compartilhado com terceiros (ver
# models/plugins/README.md). NUNCA escreve em ../../dist/ diretamente --
# quem sincroniza e o 'make install' do projeto raiz (alvo 'sync-plugins').
# ==============================================================================

ROOT      := $(abspath ../..)
PWD       := $(shell pwd)
BUILD_DIR := ./build
DEST_DIR  := $(PWD)/dist
BUILD_TYPE ?= Debug
TESTS     ?= true

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
	@# So reconfigura (caro) se o build.ninja nao existe AINDA ou se
	@# TESTS/BUILD_TYPE mudaram desde a ultima vez -- ver o "porque" no
	@# Makefile de models/flight (mesmo padrao).
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
	@WANT="$(BUILD_TYPE)|$(TESTS)"; \
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
	       $(BUILD_DIR)/ .; \
	    echo "$$WANT" > $(BUILD_DIR)/.configure-args; \
	 fi

build: configure ## Compila o .so.
	meson compile -C $(BUILD_DIR) -j$(NINJA_JOBS)

install: build ## Instala em ./dist (a raiz DESTE projeto) -- lib/mixr-plugins/libmissile.so.
	@# '--only-changed' -- ver o "porque" (medido com 'ninja -d explain') no
	@# Makefile de models/flight.
	meson install -C $(BUILD_DIR) --no-rebuild --only-changed
	@ldd $(DEST_DIR)/lib/mixr-plugins/libmissile.so | grep -q 'not found' && \
		{ echo "$(RED)install: dependencia nao resolvida$(NC)"; exit 1; } || true
	@echo "$(GREEN)install: OK$(NC) -> $(DEST_DIR)/lib/mixr-plugins/libmissile.so"

test: build ## Roda a suite de domain/ (domain::pursuit(), pura).
	meson test -C $(BUILD_DIR) --print-errorlogs

install-host: install ## Deposita em ../../models/plugins/ (flat) -- NAO toca dist/.
	@mkdir -p $(ROOT)/models/plugins
	@cp -a $(DEST_DIR)/lib/mixr-plugins/. $(ROOT)/models/plugins/
	@echo "$(GREEN)install-host: OK$(NC) -> $(ROOT)/models/plugins/"
	@echo "  (dist/ so e populado por 'cd $(ROOT) && make install')"

clean: ## Remove ./build e ./dist (LOCAIS -- nao mexe no ../../dist do host).
	rm -rf $(BUILD_DIR) $(DEST_DIR)

help:
	@grep -E '^[a-zA-Z0-9_-]+:.*?## .*$$' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-16s\033[0m %s\n", $$1, $$2}'
