.PHONY: build test install install-host uninstall-host create-bt update-bt open-groot
.DEFAULT_GOAL := help

# ==============================================================================
# Makefile AUTOCONTIDO do modelo `flight` (o modelo de producao; a PASTA chama-se
# A-4, a aeronave que ele pilota -- o nome de fabrica/biblioteca continua flight).
#
# Abra o VS Code AQUI, rode `make test` (ou qualquer alvo abaixo), e o artefato
# (libflight.so) + os dados (flight_tree.xml, data/jsbsim/) saem em ./dist -- a
# raiz DESTE projeto, nao a do poc-mixr. Nao chama o Makefile raiz.
#
# Pre-requisito, publicado pelo HOST uma vez:  cd ../../.. && make configure && make sdk
#
# 'install-host' e o unico alvo que toca o projeto raiz -- e so ate
# ../../../plugins/ (lib, flat) e ../../../plugins/data/flight/, o MESMO deposito
# que um .so de terceiro usaria (ver plugins/README.md). NUNCA escreve em
# ../../../dist/: quem sincroniza plugins/ -> dist/ e o 'make install' da raiz
# (alvo 'sync-plugins'). Sem os dois passos o .so compila mas nao roda em cenario
# nenhum.
#
# check-root/configure/clean/help sao COMPARTILHADOS -- moram em ../../common.mk.
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

build: configure ## Compila libflight.so.
	meson compile -C $(BUILD_DIR) -j$(NINJA_JOBS)

test: build ## Roda a suite do modelo (domain + tree + native).
	@N=$$(meson introspect --tests $(BUILD_DIR) | python3 -c 'import json,sys; print(len(json.load(sys.stdin)))'); \
	 [ "$$N" -ge 3 ] || { echo "$(RED)suite incompleta ($$N de 3) -- confira TESTS=true$(NC)"; exit 1; }
	meson test -C $(BUILD_DIR) --print-errorlogs

install: build ## Instala em ./dist deste projeto (lib/ + share/mixr-plugins/flight/).
	@# '--only-changed': sem isto o 'meson install' recopia com mtime NOVO mesmo
	@# com conteudo identico, e um destino mais novo que um input ja linkado (ex.:
	@# dist/lib/libxtrack.so, do SDK) faz o ninja RELINKAR na proxima chamada.
	meson install -C $(BUILD_DIR) --no-rebuild --only-changed
	@# Varre todo .so que de fato existe -- nenhum nome cravado: 'ldd' contra um
	@# caminho inexistente nao contem "not found" e o '|| true' viraria um no-op.
	@for so in $(DEST_DIR)/lib/mixr-plugins/*.so; do \
		[ -e "$$so" ] || continue; \
		ldd "$$so" | grep -q 'not found' && \
			{ echo "$(RED)install: dependencia nao resolvida em $$so$(NC)"; exit 1; } || true; \
	done
	@echo "$(GREEN)install: OK$(NC) -> $(DEST_DIR)/lib/mixr-plugins/, $(DEST_DIR)/share/mixr-plugins/flight/"

install-host: install ## Deposita em ../../../plugins/ -- nao toca dist/ do host.
	@mkdir -p $(ROOT)/plugins $(ROOT)/plugins/data
	@cp -a $(DEST_DIR)/lib/mixr-plugins/. $(ROOT)/plugins/
	@# Copia o diretorio de dados INTEIRO, sem nome cravado: o nome nao e derivavel
	@# do nome da pasta (pasta 'A-4', dados em 'flight/'), e um literal aqui tornaria
	@# a remocao do modelo impossivel de automatizar. Mesmo que 'sync-plugins' faz.
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
