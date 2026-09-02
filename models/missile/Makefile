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
# Ver models/fixtures/stub/README.md para o "porque" de `install-host` ser
# o unico alvo que escreve fora de ./dist -- mesmo padrao aqui.
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
	mkdir -p $(BUILD_DIR)
	meson setup --reconfigure \
		--backend ninja \
		--buildtype $(shell echo $(BUILD_TYPE) | tr '[:upper:]' '[:lower:]') \
		--native-file $(ROOT)/build/conan_meson_native.ini \
		--prefix=$(DEST_DIR) \
		--libdir=$(DEST_DIR)/lib \
		-Dpkg_config_path=$(PKG_PATH) \
		-Dtests=$(TESTS) \
		$(BUILD_DIR)/ .

build: configure ## Compila o .so.
	meson compile -C $(BUILD_DIR) -j$(NINJA_JOBS)

install: build ## Instala em ./dist (a raiz DESTE projeto) -- lib/mixr-plugins/libmissile.so.
	meson install -C $(BUILD_DIR) --no-rebuild
	@ldd $(DEST_DIR)/lib/mixr-plugins/libmissile.so | grep -q 'not found' && \
		{ echo "$(RED)install: dependencia nao resolvida$(NC)"; exit 1; } || true
	@echo "$(GREEN)install: OK$(NC) -> $(DEST_DIR)/lib/mixr-plugins/libmissile.so"

test: build ## Roda a suite de domain/ (domain::pursuit(), pura).
	meson test -C $(BUILD_DIR) --print-errorlogs

install-host: install ## Sincroniza ./dist para ../../dist -- o UNICO alvo que toca o host.
	@mkdir -p $(ROOT)/dist/lib/mixr-plugins
	@cp -a $(DEST_DIR)/lib/mixr-plugins/. $(ROOT)/dist/lib/mixr-plugins/
	@echo "$(GREEN)install-host: OK$(NC) -> $(ROOT)/dist/lib/mixr-plugins/"

clean: ## Remove ./build e ./dist (LOCAIS -- nao mexe no ../../dist do host).
	rm -rf $(BUILD_DIR) $(DEST_DIR)

help:
	@grep -E '^[a-zA-Z0-9_-]+:.*?## .*$$' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-16s\033[0m %s\n", $$1, $$2}'
