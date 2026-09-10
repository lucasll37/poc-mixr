.PHONY: build test install install-host uninstall-host create-bt update-bt open-groot
.DEFAULT_GOAL := help

# ==============================================================================
# Makefile AUTOCONTIDO deste template.
#
# Abra o VS Code AQUI, rode `make`, e o .so sai em ./dist -- a raiz DESTE
# projeto, nao a do poc-mixr. Nao chama o Makefile raiz, e o alvo `models` dela
# tambem nao constroi este diretorio (ver o cabecalho do meson.build; para
# copiar isto num modelo de verdade, docs/PRIMEIROS-PASSOS.md).
#
# Pre-requisito, publicado pelo HOST uma vez:  cd ../.. && make configure && make sdk
#
# 'install-host' e o unico alvo que escreve fora de ./dist -- e so ate
# ../../plugins/, o deposito compartilhado com terceiros (ver
# ../../plugins/README.md). NUNCA escreve em ../../dist/: quem sincroniza e
# o 'make install' da raiz (alvo 'sync-plugins'). Sem os dois passos o .so
# compila e testa aqui, mas nao roda em cenario nenhum.
#
# check-root/configure/clean/help sao COMPARTILHADOS -- moram em ../common.mk.
# ==============================================================================

ROOT      := $(abspath ../..)
BUILD_TYPE ?= Debug
TESTS     ?= true

YELLOW := \033[1;33m

include $(ROOT)/models/common.mk

build: configure ## Compila o .so deste projeto.
	meson compile -C $(BUILD_DIR) -j$(NINJA_JOBS)

test: build ## Roda a suite deste template (domain + tree + forma do .so).
	@# 4, e nao 5: o teste do mirror de contrato desaparece quando 'make new-model'
	@# copia este diretorio (o bloco MIRROR-BLOCK sai junto com src/mirror.cpp), e a
	@# guarda tem de valer nos dois casos.
	@N=$$(meson introspect --tests $(BUILD_DIR) | python3 -c 'import json,sys; print(len(json.load(sys.stdin)))'); \
	 [ "$$N" -ge 4 ] || { echo "$(RED)suite incompleta ($$N de 4) -- confira TESTS=true$(NC)"; exit 1; }
	meson test -C $(BUILD_DIR) --print-errorlogs

install: build ## Instala em ./dist deste projeto (lib/ + share/mixr-plugins/).
	@# '--only-changed' -- ver o "porque" no Makefile de models/players/A-4.
	@# Varre todo .so que de fato existe, sem nome cravado: depois de renomear o
	@# projeto ele deixa de se chamar libtemplate.so, e um 'ldd' contra um caminho
	@# inexistente nao contem "not found" -- o '|| true' absorveria o erro e a linha
	@# seguinte imprimiria "install: OK" apontando pra nada.
	meson install -C $(BUILD_DIR) --no-rebuild --only-changed
	@for so in $(DEST_DIR)/lib/mixr-plugins/*.so; do \
		[ -e "$$so" ] || continue; \
		ldd "$$so" | grep -q 'not found' && \
			{ echo "$(RED)install: dependencia nao resolvida em $$so$(NC)"; exit 1; } || true; \
	done
	@echo "$(GREEN)install: OK$(NC) -> $(DEST_DIR)/lib/mixr-plugins/, $(DEST_DIR)/share/mixr-plugins/"

install-host: install ## Deposita em ../../plugins/ -- nao toca dist/ do host.
	@mkdir -p $(ROOT)/plugins $(ROOT)/plugins/data
	@cp -a $(DEST_DIR)/lib/mixr-plugins/. $(ROOT)/plugins/
	@# A arvore vai JUNTO: o 'treeFile:' do ( ExampleBehavior ) aponta pra ca, e os
	@# nos que ela referencia so existem dentro deste .so.
	@#
	@# Copia o diretorio INTEIRO, sem nome cravado: 'make new-model' renomeia o
	@# literal 'template' no meson.build (o install_data passa a escrever em
	@# share/mixr-plugins/<seu-modelo>/) mas NAO mexe neste Makefile.
	@if [ -d $(DEST_DIR)/share/mixr-plugins ]; then \
	   cp -a $(DEST_DIR)/share/mixr-plugins/. $(ROOT)/plugins/data/; \
	 fi
	@echo "$(GREEN)install-host: OK$(NC) -> $(ROOT)/plugins/, $(ROOT)/plugins/data/"
	@echo "  (dist/ so e populado por 'cd $(ROOT) && make install')"

uninstall-host: ## Remove de ../../plugins/ o que ESTE modelo publicou.
	@if [ -d $(DEST_DIR)/lib/mixr-plugins ]; then \
	   for so in $(DEST_DIR)/lib/mixr-plugins/*.so; do \
	      [ -e "$$so" ] || continue; \
	      rm -f "$(ROOT)/plugins/$$(basename $$so)"; \
	   done; \
	 fi
	@# Os nomes saem do PROPRIO ./dist local -- so o que ESTE modelo publicou e
	@# removido, nunca o de um terceiro que divida o deposito.
	@if [ -d $(DEST_DIR)/share/mixr-plugins ]; then \
	   for d in $(DEST_DIR)/share/mixr-plugins/*/; do \
	      [ -d "$$d" ] || continue; \
	      rm -rf "$(ROOT)/plugins/data/$$(basename $$d)"; \
	   done; \
	 fi

create-bt: build ## Cria configs/bt.xml (arvore vazia + <TreeNodesModel>) pro Groot.
	@test ! -e configs/bt.xml || { \
		echo "$(RED)configs/bt.xml ja existe -- remova ou renomeie antes de rodar create-bt$(NC)"; exit 1; }
	@$(BUILD_DIR)/tools/dump-tree-model --skeleton MainTree > configs/bt.xml
	@echo "$(GREEN)create-bt: OK$(NC) -> configs/bt.xml"

update-bt: build ## Sincroniza o <TreeNodesModel> de toda arvore de configs/ com a factory.
	python3 tools/update_bt_models.py --binary $(BUILD_DIR)/tools/dump-tree-model

open-groot: ## Abre o Groot (editor/monitor de arvores BT.CPP) -- ver INSTALL.md secao 4.
	@GROOT_BIN="$$($(ROOT)/scripts/find_groot.sh 2>/dev/null)"; \
	if [ -n "$$GROOT_BIN" ] && [ -x "$$GROOT_BIN" ]; then \
		setsid "$$GROOT_BIN" >/dev/null 2>&1 & \
	else \
		echo "$(YELLOW)open-groot:$(NC) pacote groot/1.0.0 nao encontrado no cache Conan."; \
	fi
