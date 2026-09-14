.PHONY: build test create-bt update-bt open-groot
.DEFAULT_GOAL := help

# ==============================================================================
# Makefile AUTOCONTIDO do modelo `A-4` (o modelo de producao, nomeado pela
# aeronave que ele pilota -- nome de pasta, de fabrica e de biblioteca iguais).
#
# Abra o VS Code AQUI, rode `make test` (ou qualquer alvo abaixo), e o artefato
# (libA-4.so) + os dados (flight_tree.xml, data/jsbsim/) saem em ./dist -- a
# raiz DESTE projeto, nao a do poc-mixr. Nao chama o Makefile raiz.
#
# Pre-requisito, publicado pelo CORE uma vez:  cd ../../.. && make configure && make sdk
#
# 'install-core' e o unico alvo que toca o projeto raiz -- e so ate
# ../../../plugins/ (lib, flat) e ../../../plugins/data/A-4/, o MESMO deposito
# que um .so de terceiro usaria (ver plugins/README.md). NUNCA escreve em
# ../../../dist/: quem sincroniza plugins/ -> dist/ e o 'make install' da raiz
# (alvo 'sync-plugins'). Sem os dois passos o .so compila mas nao roda em cenario
# nenhum.
#
# check-root/configure/clean/help/install/install-core/uninstall-core sao
# COMPARTILHADOS -- moram em ../../common.mk.
# ==============================================================================

ROOT      := $(abspath ../../..)
BUILD_TYPE ?= Debug
TESTS     ?= true
VARIANTS  ?= false
ASAN      ?= false

EXTRA_MESON_OPTS := -Dvariants=$(VARIANTS) -Dasan=$(ASAN)
EXTRA_STALE_KEY  := $(VARIANTS)|$(ASAN)

YELLOW := \033[1;33m

include $(ROOT)/models/common.mk

build: configure ## Compila libA-4.so.
	meson compile -C $(BUILD_DIR) -j$(NINJA_JOBS)

test: build ## Roda a suite do modelo (domain + tree + native + contract).
	@N=$$(meson introspect --tests $(BUILD_DIR) | python3 -c 'import json,sys; print(len(json.load(sys.stdin)))'); \
	 [ "$$N" -ge 5 ] || { echo "$(RED)suite incompleta ($$N de 5) -- confira TESTS=true$(NC)"; exit 1; }
	meson test -C $(BUILD_DIR) --print-errorlogs

create-bt: build ## Cria configs/bt.xml (arvore vazia + <TreeNodesModel>) pro Groot.
	@test ! -e configs/bt.xml || { \
		echo "$(RED)configs/bt.xml ja existe -- remova ou renomeie antes de rodar create-bt$(NC)"; exit 1; }
	@$(BUILD_DIR)/tools/dump-tree-model --skeleton MainTree > configs/bt.xml
	@echo "$(GREEN)create-bt: OK$(NC) -> configs/bt.xml"

update-bt: build ## Sincroniza o <TreeNodesModel> de toda arvore de configs/ com a factory.
	python3 tools/update_bt_models.py --binary $(BUILD_DIR)/tools/dump-tree-model

# Log em $(ROOT)/build/groot.log -- o MESMO arquivo do 'open-groot' da raiz, de
# proposito: nao importa por qual Makefile o Groot foi aberto, o rastro fica num
# lugar so'. Ver a armadilha no 3 da secao "Groot" do CLAUDE.md -- este alvo
# mandava a saida do Groot para /dev/null e, com ela, a unica pista de que ele
# tinha ABORTADO em vez de ser fechado. FG=1 roda em primeiro plano.
open-groot: ## Abre o Groot (editor/monitor BT.CPP). Log em <raiz>/build/groot.log; FG=1 roda no terminal.
	@GROOT_LOG="$(ROOT)/build/groot.log"; \
	GROOT_BIN="$$($(ROOT)/scripts/find_groot.sh)"; \
	if [ -n "$$GROOT_BIN" ] && [ -x "$$GROOT_BIN" ]; then \
		mkdir -p "$$(dirname "$$GROOT_LOG")"; \
		{ echo; echo "=== $$(date -Is) -- $$GROOT_BIN"; } >> "$$GROOT_LOG"; \
		if [ -n "$$FG" ]; then \
			"$$GROOT_BIN" 2>&1 | tee -a "$$GROOT_LOG"; \
		else \
			setsid "$$GROOT_BIN" >> "$$GROOT_LOG" 2>&1 & \
			echo "open-groot: saida (inclusive um fechamento inesperado) em $$GROOT_LOG"; \
		fi; \
	else \
		echo "$(YELLOW)open-groot:$(NC) pacote groot/1.0.0 nao encontrado no cache Conan."; \
	fi
