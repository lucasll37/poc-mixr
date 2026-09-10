.PHONY: build test install install-host uninstall-host create-bt update-bt open-groot
.DEFAULT_GOAL := help

# ==============================================================================
# Makefile AUTOCONTIDO do modelo C-130.
#
# "Autocontido" quer dizer: abra o VS Code AQUI (models/players/C-130/), rode
# `make`, e o .so sai em ./dist -- a raiz DESTE projeto, nao a raiz do
# poc-mixr. Este Makefile nunca chama o Makefile raiz -- mas o INVERSO
# acontece: o alvo `models` da raiz DESCOBRE este projeto por `find` (todo
# diretorio sob models/ com um project() no meson.build, exceto
# models/players/template/) e builda/instala ele junto com A-4.
#
# Pre-requisito, publicado pelo projeto HOST uma vez:
#
#     cd ../../.. && make configure && make sdk
#
# Depois disso este Makefile roda sozinho, quantas vezes quiser, so contra o
# que foi publicado em ../../../dist e ../../../build.
#
# `install-host` e o unico alvo que escreve fora de ./dist -- mas so ate
# ../../../plugins/, o deposito compartilhado com terceiros (ver
# ../../../plugins/README.md). NUNCA escreve em ../../../dist/ diretamente --
# quem sincroniza e o 'make install' do projeto raiz (alvo 'sync-plugins').
# Sem os dois passos, o .so fica so aqui -- util para iterar (compilar,
# rodar a suite), inutilizavel por um cenario ate ser publicado.
#
# check-root/configure/clean/help sao COMPARTILHADOS com models/players/A-4/
# Makefile -- moram em ../../common.mk. Este arquivo so acrescenta o que e
# proprio deste modelo.
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

install: build ## Instala em ./dist deste projeto (lib/ + share/mixr-plugins/).
	@# '--only-changed' -- ver o "porque" (medido com 'ninja -d explain') no
	@# Makefile de models/players/A-4.
	@# NAO hardcodear o nome do .so aqui: depois de renomear o projeto (ver
	@# docs/PRIMEIROS-PASSOS.md), ele deixa de se chamar libC-130.so, e um
	@# 'ldd' contra um caminho que nao existe mais sai (exit 1, sem a string
	@# "not found") -- o '|| true' absorvia isso e a linha seguinte imprimia
	@# "install: OK" apontando pra um arquivo inexistente, incondicionalmente.
	meson install -C $(BUILD_DIR) --no-rebuild --only-changed
	@for so in $(DEST_DIR)/lib/mixr-plugins/*.so; do \
		[ -e "$$so" ] || continue; \
		ldd "$$so" | grep -q 'not found' && \
			{ echo "$(RED)install: dependencia nao resolvida em $$so$(NC)"; exit 1; } || true; \
	done
	@echo "$(GREEN)install: OK$(NC) -> $(DEST_DIR)/lib/mixr-plugins/, $(DEST_DIR)/share/mixr-plugins/"

install-host: install ## Deposita em ../../../plugins/ -- nao toca dist/ do host.
	@mkdir -p $(ROOT)/plugins $(ROOT)/plugins/data
	@cp -a $(DEST_DIR)/lib/mixr-plugins/. $(ROOT)/plugins/
	@# A arvore vai JUNTO: o 'treeFile:' do ( ExampleBehavior ) aponta pra
	@# ca, e os nos que ela referencia so existem dentro deste .so. Mesmo
	@# motivo (e mesmo destino) do install_data em meson.build.
	@#
	@# NENHUM nome de modelo escrito aqui, de proposito (ACHADO RODANDO, nao
	@# redescobrir): 'make new-model' renomeia o literal 'template' dentro do
	@# meson.build -- entao o install_data passa a escrever em
	@# share/mixr-plugins/<seu-modelo>/ -- mas NAO mexe neste Makefile. Um
	@# caminho com 'template' cravado aqui viraria "cp: cannot stat .../
	@# template/: No such file or directory" no primeiro install-host do
	@# scaffold copiado (reproduzido). Copiar o diretorio INTEIRO resolve
	@# sozinho, para qualquer nome, e e' exatamente o que o 'sync-plugins'
	@# da raiz ja faz um nivel acima.
	@if [ -d $(DEST_DIR)/share/mixr-plugins ]; then \
	   cp -a $(DEST_DIR)/share/mixr-plugins/. $(ROOT)/plugins/data/; \
	 fi
	@echo "$(GREEN)install-host: OK$(NC) -> $(ROOT)/plugins/, $(ROOT)/plugins/data/"
	@echo "  (dist/ so e populado por 'cd $(ROOT) && make install')"

uninstall-host: ## Remove de ../../../plugins/ o que ESTE modelo publicou.
	@if [ -d $(DEST_DIR)/lib/mixr-plugins ]; then \
	   for so in $(DEST_DIR)/lib/mixr-plugins/*.so; do \
	      [ -e "$$so" ] || continue; \
	      rm -f "$(ROOT)/plugins/$$(basename $$so)"; \
	   done; \
	 fi
	@# Mesmo raciocinio do install-host: os nomes saem do PROPRIO ./dist
	@# local, nunca escritos a mao -- so' o que ESTE modelo publicou e'
	@# removido, nunca o dado de um terceiro que divida o deposito.
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
