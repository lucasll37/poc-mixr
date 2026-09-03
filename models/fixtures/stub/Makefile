.PHONY: all configure build test install clean check-root help
.DEFAULT_GOAL := help

# ==============================================================================
# Makefile AUTOCONTIDO do modelo `stub`.
#
# "Autocontido" quer dizer: abra o VS Code AQUI (models/fixtures/stub/), rode
# `make`, e o .so (+ o que mais for preciso) sai em ./dist -- a raiz DESTE
# projeto, nao a raiz do poc-mixr. Nao chama o Makefile raiz nem o alvo
# `models` dele.
#
# O que continua sendo pre-requisito, e nao tem como deixar de ser: o SDK de
# plugin (o contrato + libxboard/libxlog/libxtrack) e os .pc do Conan (mixr,
# behaviortree.cpp.asa, gtest...) sao publicados pelo projeto HOST, uma vez:
#
#     cd ../../.. && make configure && make sdk
#
# Depois disso, este Makefile nunca mais precisa do Makefile raiz -- roda
# tantas vezes quanto quiser, sozinho, so contra o que foi publicado em
# ../../../dist e ../../../build.
#
# `make install-host` e o unico alvo que TOCA no projeto raiz: deposita
# este ./dist local em ../../../models/plugins/, o MESMO deposito flat que
# um .so de terceiro usaria (ver models/plugins/README.md). NUNCA escreve
# em ../../../dist/ diretamente -- quem sincroniza models/plugins/ ->
# dist/lib/mixr-plugins/ e o 'make install' do projeto raiz (alvo
# 'sync-plugins'). Sem os dois passos, o .so fica so aqui -- util para
# iterar (compilar, rodar a suite de contrato) sem afetar o host, mas
# inutilizavel por um cenario ate ser publicado.
# ==============================================================================

ROOT      := $(abspath ../../..)
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
	@WANT="$(BUILD_TYPE)|$(TESTS)"; \
	 GOT=$$(cat $(BUILD_DIR)/.configure-args 2>/dev/null || echo ""); \
	 STALE=$$(find meson.build meson_options.txt $(ROOT)/build/conan_meson_native.ini -newer $(BUILD_DIR)/build.ninja 2>/dev/null); \
	 if [ -f $(BUILD_DIR)/build.ninja ] && [ "$$WANT" = "$$GOT" ] && [ -z "$$STALE" ]; then \
	    :; \
	 else \
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

install: build ## Instala em ./dist (a raiz DESTE projeto) -- lib/mixr-plugins/libstub.so.
	@# '--only-changed' -- ver o "porque" (medido com 'ninja -d explain') no
	@# Makefile de models/flight.
	meson install -C $(BUILD_DIR) --no-rebuild --only-changed
	@ldd $(DEST_DIR)/lib/mixr-plugins/libstub.so | grep -q 'not found' && \
		{ echo "$(RED)install: dependencia nao resolvida$(NC)"; exit 1; } || true
	@echo "$(GREEN)install: OK$(NC) -> $(DEST_DIR)/lib/mixr-plugins/libstub.so"

test: build ## Roda a suite de CONTRATO deste projeto (forma do .so -- nao o cenario de producao).
	meson test -C $(BUILD_DIR) --print-errorlogs

install-host: install ## Deposita em ../../../models/plugins/ (flat) -- NAO toca dist/.
	@mkdir -p $(ROOT)/models/plugins
	@cp -a $(DEST_DIR)/lib/mixr-plugins/. $(ROOT)/models/plugins/
	@echo "$(GREEN)install-host: OK$(NC) -> $(ROOT)/models/plugins/"
	@echo "  'cd $(ROOT) && make install' sincroniza para dist/; so ai um cenario enxerga libstub.so."

clean: ## Remove ./build e ./dist (LOCAIS -- nao mexe no ../../../dist do host).
	rm -rf $(BUILD_DIR) $(DEST_DIR)

help:
	@grep -E '^[a-zA-Z0-9_-]+:.*?## .*$$' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-16s\033[0m %s\n", $$1, $$2}'
