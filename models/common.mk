# ==============================================================================
# Logica compartilhada entre os Makefiles dos modelos (models/<categoria>/<nome>/).
# E' 'include'do, nunca chamado direto.
#
# AQUI: as variaveis comuns + clean/check-root/configure/help.
# NO MAKEFILE-FILHO: build/test/install/install-host/uninstall-host (a lista de
# .so publicada e o diretorio de dados sao por-modelo) e create-bt/update-bt/
# open-groot (dependem de tools/dump-tree-model, que so existe num modelo que de
# fato tenha uma arvore de comportamento). O filho os declara DEPOIS do include.
#
# Contrato -- definir ANTES do 'include':
#   ROOT              obrigatorio. $(abspath ../../..) ou equivalente para a
#                     profundidade do destino. A linha mora no Makefile-FILHO,
#                     nunca aqui: 'scripts/models.sh::linha_root()' a reescreve a
#                     cada 'make new-model'. Move-la pra ca quebraria isso.
#   BUILD_TYPE ?=     default 'Debug'.
#   TESTS ?=          default 'true'.
#   EXTRA_MESON_OPTS  opcional -- flags extras de 'meson setup' (ex.: o A-4 usa
#                     '-Dvariants=$(VARIANTS) -Dasan=$(ASAN)').
#   EXTRA_STALE_KEY   opcional -- entra na chave de cache de reconfiguracao, para
#                     uma mudanca em EXTRA_MESON_OPTS disparar reconfigure.
#
# 'help' do filho continua funcionando: o GNU Make ACUMULA $(MAKEFILE_LIST), e o
# grep abaixo varre a lista inteira, nao so este arquivo.
# ==============================================================================

.PHONY: clean check-root configure help
.DEFAULT_GOAL := help

PWD        := $(shell pwd)
BUILD_DIR  := ./build
DEST_DIR   := $(PWD)/dist
PKG_PATH   := $(ROOT)/dist/lib/pkgconfig,$(ROOT)/build
NINJA_JOBS := $(shell nproc)

RED   := \033[0;31m
GREEN := \033[0;32m
NC    := \033[0m

EXTRA_MESON_OPTS ?=
EXTRA_STALE_KEY  ?=

clean: ## Remove ./build e ./dist LOCAIS -- nao mexe no dist/ do host.
	rm -rf $(BUILD_DIR) $(DEST_DIR)

check-root: ## Confere que o host ja publicou o SDK (pre-requisito, uma vez).
	@test -f $(ROOT)/build/conan_meson_native.ini || { \
		echo "$(RED)faltando $(ROOT)/build/conan_meson_native.ini$(NC)"; \
		echo "  rode 'cd $(ROOT) && make configure' primeiro (uma vez)."; exit 1; }
	@test -f $(ROOT)/dist/lib/pkgconfig/poc-mixr-sdk.pc || { \
		echo "$(RED)faltando o SDK de plugin em $(ROOT)/dist$(NC)"; \
		echo "  rode 'cd $(ROOT) && make sdk' primeiro (uma vez)."; exit 1; }
	@echo "$(GREEN)check-root: OK$(NC) -> SDK do host publicado em $(ROOT)/dist"

configure: check-root ## meson setup isolado em ./build, consumindo o SDK do host.
	@mkdir -p $(BUILD_DIR)
	@# Reconfigurar e caro (~1-2s, reavalia TODAS as dependencias), entao so
	@# roda se build.ninja ainda nao existe, se BUILD_TYPE/TESTS/EXTRA_STALE_KEY
	@# mudaram, ou se algum meson.build/meson_options.txt (recursivo), o
	@# native-file do Conan ou o poc-mixr-sdk.pc ficarem mais novos que ele.
	@#
	@# Os tres ultimos entram no STALE porque o ninja tem regen AUTOMATICO
	@# proprio (dispara sozinho, independente deste guard) e ele NAO repassa
	@# '-Dpkg_config_path' -- o sintoma e o build do modelo falhar com
	@# "Dependency poc-mixr-sdk not found" com o .pc presente em dist/. O .pc
	@# em si entra porque o meson resolve o 'Libs:' dele em tempo de CONFIGURE:
	@# uma lib NOVA no SDK nao apareceria, e o link falharia com "undefined
	@# reference" a uma funcao que existe e esta exportada.
	@#
	@# '--clearcache' antes do setup: o meson cacheia o resultado do pkg-config
	@# entre execucoes, e '--reconfigure' sozinho reusa o cache. (E argumento de
	@# 'meson configure', nao de 'meson setup' -- passa-lo ao setup faz o meson
	@# recusar a linha inteira.)
	@#
	@# '--reconfigure' e CONDICIONAL: exige um build tree ja existente e aborta
	@# sem ele ("Directory does not contain a valid build tree") -- o caso de um
	@# 'make models' logo depois de um 'make clean'. Com o tree presente ela e
	@# OBRIGATORIA, senao o meson so imprime "Directory already configured" e
	@# sai 0. O teste e por meson-private/coredata.dat, o MESMO arquivo que o
	@# meson consulta -- nao pelo build.ninja do STALE-check acima.
	@WANT="$(BUILD_TYPE)|$(TESTS)|$(EXTRA_STALE_KEY)"; \
	 GOT=$$(cat $(BUILD_DIR)/.configure-args 2>/dev/null || echo ""); \
	 STALE=$$(find . \( -name 'meson.build' -o -name 'meson_options.txt' \) \
	             -not -path './$(BUILD_DIR)/*' -newer $(BUILD_DIR)/build.ninja 2>/dev/null; \
	           find $(ROOT)/build/conan_meson_native.ini $(ROOT)/dist/lib/pkgconfig/poc-mixr-sdk.pc \
	             -newer $(BUILD_DIR)/build.ninja 2>/dev/null); \
	 if [ -f $(BUILD_DIR)/build.ninja ] && [ "$$WANT" = "$$GOT" ] && [ -z "$$STALE" ]; then \
	    :; \
	 else \
	    if [ -f $(BUILD_DIR)/build.ninja ]; then \
	       meson configure $(BUILD_DIR) --clearcache >/dev/null 2>&1 || true; \
	    fi; \
	    if [ -f $(BUILD_DIR)/meson-private/coredata.dat ]; \
	       then RECONF=--reconfigure; else RECONF=; fi; \
	    meson setup $$RECONF \
	       --backend ninja \
	       --buildtype $(shell echo $(BUILD_TYPE) | tr '[:upper:]' '[:lower:]') \
	       --native-file $(ROOT)/build/conan_meson_native.ini \
	       --prefix=$(DEST_DIR) \
	       --libdir=$(DEST_DIR)/lib \
	       -Dpkg_config_path=$(PKG_PATH) \
	       -Dtests=$(TESTS) \
	       $(EXTRA_MESON_OPTS) \
	       $(BUILD_DIR)/ .; \
	    echo "$$WANT" > $(BUILD_DIR)/.configure-args; \
	 fi

help: ## Lista os alvos deste Makefile (e' o que 'make' sem alvo roda).
	@# 'grep -h': $(MAKEFILE_LIST) tem mais de um arquivo (o Makefile-filho +
	@# este), e o grep multi-arquivo padrao prefixaria cada linha com
	@# "<arquivo>:", empurrando o nome do ALVO pra fora da primeira coluna.
	@grep -hE '^[a-zA-Z0-9_-]+:.*?## .*$$' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-16s\033[0m %s\n", $$1, $$2}'
