.PHONY: build test create-bt update-bt open-groot
.DEFAULT_GOAL := help

# ==============================================================================
# Makefile AUTOCONTIDO do modelo C-130.
#
# Abra o VS Code AQUI (models/players/C-130/), rode `make`, e o .so sai em ./dist -- a
# raiz DESTE projeto, nao a do poc-mixr. Este Makefile nunca chama o Makefile
# raiz; o INVERSO acontece -- o alvo `models` da raiz descobre este projeto por
# `find` (todo diretorio sob models/ com project() no meson.build, exceto
# models/template/) e builda/instala ele junto com os demais.
#
# Pre-requisito, publicado pelo CORE uma vez:  cd ../../.. && make configure && make sdk
#
# 'install-core' e o unico alvo que escreve fora de ./dist -- e so ate
# ../../../plugins/, o deposito compartilhado com terceiros (ver
# ../../../plugins/README.md). NUNCA escreve em ../../../dist/: quem sincroniza e
# o 'make install' da raiz (alvo 'sync-plugins'). Sem os dois passos o .so
# compila e testa aqui, mas nao roda em cenario nenhum.
#
# check-root/configure/clean/help/install/install-core/uninstall-core sao
# COMPARTILHADOS -- moram em ../../common.mk.
# ==============================================================================

ROOT      := $(abspath ../../..)
BUILD_TYPE ?= Debug
TESTS     ?= true

YELLOW := \033[1;33m

include $(ROOT)/models/common.mk

build: configure ## Compila o .so deste projeto.
	meson compile -C $(BUILD_DIR) -j$(NINJA_JOBS)

test: build ## Roda a suite do C-130 (domain + tree + native + forma do .so).
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
