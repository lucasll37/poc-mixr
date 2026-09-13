.PHONY: build test install install-host uninstall-host
.DEFAULT_GOAL := help

# ==============================================================================
# Makefile AUTOCONTIDO de Beacon.
#
# Abra o VS Code AQUI, rode `make`, e o .so sai em ./dist -- a raiz DESTE
# projeto, nao a do poc-mixr. Nao chama o Makefile raiz, e o alvo `models` dela
# tambem nao constroi este diretorio a menos que este projeto exista sob
# models/<categoria>/ (ver o cabecalho do meson.build).
#
# Pre-requisito, publicado pelo HOST uma vez:  cd ../../.. && make configure && make sdk
#
# 'install-host' e o unico alvo que escreve fora de ./dist -- e so ate
# ../../../plugins/, o deposito compartilhado com terceiros (ver
# ../../../plugins/README.md). NUNCA escreve em ../../../dist/: quem sincroniza e
# o 'make install' da raiz (alvo 'sync-plugins'). Sem os dois passos o .so
# compila e testa aqui, mas nao roda em cenario nenhum.
#
# check-root/configure/clean/help/lint sao COMPARTILHADOS -- moram em ../../common.mk.
#
# SEM create-bt/update-bt/open-groot: este modelo nao tem arvore de
# comportamento nenhuma (ver o cabecalho de src/plugin.cpp) -- os tres
# dependem de tools/dump-tree-model, que so existe num projeto que de fato
# tenha uma arvore (ver o comentario de ../../common.mk sobre por que esses
# tres alvos moram no Makefile-FILHO, nao no compartilhado).
# ==============================================================================

ROOT      := $(abspath ../../..)
BUILD_TYPE ?= Debug
TESTS     ?= true

include $(ROOT)/models/common.mk

build: configure ## Compila o .so deste projeto.
	meson compile -C $(BUILD_DIR) -j$(NINJA_JOBS)

test: build ## Roda a suite deste modelo (native + forma do .so).
	@# 2, nao 4: Beacon nao tem domain/tree separados (uma classe so', sem
	@# arvore de comportamento) -- so' 'native' (a classe Beacon de verdade,
	@# sem Station) e 'contract' (a forma do .so publicado).
	@N=$$(meson introspect --tests $(BUILD_DIR) | python3 -c 'import json,sys; print(len(json.load(sys.stdin)))'); \
	 [ "$$N" -ge 2 ] || { echo "$(RED)suite incompleta ($$N de 2) -- confira TESTS=true$(NC)"; exit 1; }
	meson test -C $(BUILD_DIR) --print-errorlogs

install: build ## Instala em ./dist deste projeto (lib/mixr-plugins/).
	@# '--only-changed' -- ver o "porque" no Makefile de models/players/A-4.
	@# Varre todo .so que de fato existe, sem nome cravado.
	meson install -C $(BUILD_DIR) --no-rebuild --only-changed
	@for so in $(DEST_DIR)/lib/mixr-plugins/*.so; do \
		[ -e "$$so" ] || continue; \
		ldd "$$so" | grep -q 'not found' && \
			{ echo "$(RED)install: dependencia nao resolvida em $$so$(NC)"; exit 1; } || true; \
	done
	@echo "$(GREEN)install: OK$(NC) -> $(DEST_DIR)/lib/mixr-plugins/"

install-host: install ## Deposita em ../../../plugins/ -- nao toca dist/ do host.
	@mkdir -p $(ROOT)/plugins
	@cp -a $(DEST_DIR)/lib/mixr-plugins/. $(ROOT)/plugins/
	@echo "$(GREEN)install-host: OK$(NC) -> $(ROOT)/plugins/"
	@echo "  (dist/ so e populado por 'cd $(ROOT) && make install')"

uninstall-host: ## Remove de ../../../plugins/ o que ESTE modelo publicou.
	@if [ -d $(DEST_DIR)/lib/mixr-plugins ]; then \
	   for so in $(DEST_DIR)/lib/mixr-plugins/*.so; do \
	      [ -e "$$so" ] || continue; \
	      rm -f "$(ROOT)/plugins/$$(basename $$so)"; \
	   done; \
	 fi
