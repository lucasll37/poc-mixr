.PHONY: build test install install-host uninstall-host
.DEFAULT_GOAL := help

# ==============================================================================
# Makefile AUTOCONTIDO deste template.
#
# "Autocontido" quer dizer: abra o VS Code AQUI (models/players/template/), rode
# `make`, e o .so sai em ./dist -- a raiz DESTE projeto, nao a raiz do
# poc-mixr. Nao chama o Makefile raiz nem o alvo `models` dele -- e por
# desenho: este diretorio NUNCA e construido pelo `make models` da raiz (ver
# o cabecalho de meson.build para o "porque" e docs/PRIMEIROS-PASSOS.md para
# como copiar isto para um modelo de verdade, que ai sim entraria naquele
# alvo).
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
# Makefile -- moram em ../../common.mk (o texto era ~65-70 linhas identicas
# entre os dois, e ja tinha comecado a divergir em REDACAO). Este arquivo so
# acrescenta o que e proprio deste template: o .so unico e sem dados extras.
# ==============================================================================

ROOT      := $(abspath ../../..)
BUILD_TYPE ?= Debug
TESTS     ?= true

include $(ROOT)/models/common.mk

build: configure ## Compila o .so deste projeto.
	meson compile -C $(BUILD_DIR) -j$(NINJA_JOBS)

test: build ## Roda a suite deste template (domain + forma do .so).
	@N=$$(meson introspect --tests $(BUILD_DIR) | python3 -c 'import json,sys; print(len(json.load(sys.stdin)))'); \
	 [ "$$N" -ge 2 ] || { echo "$(RED)suite incompleta ($$N de 2) -- confira TESTS=true$(NC)"; exit 1; }
	meson test -C $(BUILD_DIR) --print-errorlogs

install: build ## Instala em ./dist deste projeto (lib/mixr-plugins/).
	@# '--only-changed' -- ver o "porque" (medido com 'ninja -d explain') no
	@# Makefile de models/players/A-4.
	@# NAO hardcodear o nome do .so aqui: depois de renomear o projeto (ver
	@# docs/PRIMEIROS-PASSOS.md), ele deixa de se chamar libtemplate.so, e um
	@# 'ldd' contra um caminho que nao existe mais sai (exit 1, sem a string
	@# "not found") -- o '|| true' absorvia isso e a linha seguinte imprimia
	@# "install: OK" apontando pra um arquivo inexistente, incondicionalmente.
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
	@echo "  (dist/ so e populado por 'cd $(ROOT) && make install' -- nenhum cenario"
	@echo "   declara 'libtemplate.so'/'libtemplate_mirror.so' em 'file:' diretamente;"
	@echo "   o segundo e carregado so pelos testes de plugin do host, trocando o"
	@echo "   'file:' do cenario de producao em tempo de execucao)"

uninstall-host: ## Remove de ../../../plugins/ o que ESTE modelo publicou.
	@if [ -d $(DEST_DIR)/lib/mixr-plugins ]; then \
	   for so in $(DEST_DIR)/lib/mixr-plugins/*.so; do \
	      [ -e "$$so" ] || continue; \
	      rm -f "$(ROOT)/plugins/$$(basename $$so)"; \
	   done; \
	 fi
