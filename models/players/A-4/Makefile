.PHONY: build test install install-host uninstall-host create-bt update-bt open-groot
.DEFAULT_GOAL := help

# ==============================================================================
# Makefile AUTOCONTIDO do modelo `flight` (o modelo de producao).
#
# "Autocontido" quer dizer: abra o VS Code AQUI (models/players/A-4/), rode
# `make test` (ou qualquer alvo abaixo), e os dois artefatos (libflight.so /
# libflight_tc.so) + os dados (flight_tree.xml, data/jsbsim/) saem em
# ./dist -- a raiz DESTE projeto, nao a raiz do poc-mixr. Nao chama o
# Makefile raiz nem o alvo `models` dele.
#
# 'make' SEM alvo roda 'help' (.DEFAULT_GOAL acima) -- mesmo padrao do
# Makefile raiz do poc-mixr: lista os alvos com a descricao de cada um, nao
# compila nada por acidente.
#
# Pre-requisito, publicado pelo projeto HOST uma vez:
#
#     cd ../../.. && make configure && make sdk
#
# Depois disso este Makefile roda sozinho, quantas vezes quiser, so contra o
# que foi publicado em ../../../dist e ../../../build.
#
# 'make install-host' e o unico alvo que toca o projeto raiz -- mas so ate
# ../../../plugins/ (lib, flat) e ../../../plugins/data/flight/
# (arvore + aeronave), o MESMO deposito que um .so de terceiro usaria (ver
# plugins/README.md). NUNCA escreve em ../../../dist/ diretamente --
# quem sincroniza plugins/ -> dist/lib(share)/mixr-plugins/ e o
# 'make install' do projeto raiz (alvo 'sync-plugins'), no HOST, no momento
# em que o HOST de fato instala. Sem rodar 'make install-host' aqui E
# 'make install' na raiz, compilar e util para iterar (ver 'make test') mas
# nao roda em cenario nenhum -- ver models/players/fixtures/stub/README.md para o
# mesmo "porque" aplicado la.
#
# check-root/configure/clean/help sao COMPARTILHADOS com models/players/
# template/Makefile -- moram em ../../common.mk (o texto era ~65-70 linhas
# identicas entre os dois, e ja tinha comecado a divergir em REDACAO). Este
# arquivo so acrescenta o que e genuinamente proprio de flight: os dois
# artefatos, os dados, e create-bt/update-bt/open-groot.
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

build: configure ## Compila libflight.so + libflight_tc.so.
	meson compile -C $(BUILD_DIR) -j$(NINJA_JOBS)

test: build ## Roda a suite do modelo (domain + tree + native).
	@N=$$(meson introspect --tests $(BUILD_DIR) | python3 -c 'import json,sys; print(len(json.load(sys.stdin)))'); \
	 [ "$$N" -ge 3 ] || { echo "$(RED)suite incompleta ($$N de 3) -- confira TESTS=true$(NC)"; exit 1; }
	meson test -C $(BUILD_DIR) --print-errorlogs

install: build ## Instala em ./dist deste projeto (lib/ + share/mixr-plugins/flight/).
	@# '--only-changed': sem isto, 'meson install' recopia e atualiza o mtime
	@# do destino MESMO com conteudo identico -- e um destino mais novo que
	@# um INPUT ja linkado (ex.: dist/lib/libxtrack.so, do SDK, do qual
	@# libflight.so depende) faz o ninja achar que precisa RELINKAR na
	@# proxima chamada, mesmo sem nada ter mudado de verdade -- confirmado
	@# com 'ninja -C build -d explain libflight.so': "output libflight.so
	@# older than most recent input .../libxtrack.so". Sem isto, 'make
	@# run-*'/'make test' relinkavam os quatro plugins TODA vez, mesmo com
	@# tudo ja compilado.
	meson install -C $(BUILD_DIR) --no-rebuild --only-changed
	@# ACHADO POR AUDITORIA, CORRIGIDO (nao redescobrir): a lista fixa
	@# 'libflight.so libflight_tc.so' sobrevivia da epoca de DOIS artefatos
	@# separados -- desde a unificacao num artefato so (libflight.so, ver o
	@# comentario do meson.build deste projeto), 'libflight_tc.so' nunca
	@# existe mais em $(DEST_DIR). 'ldd' contra um caminho inexistente sai
	@# com "No such file or directory" (nao contem a string "not found"), o
	@# '|| true' absorvia isso e a linha virava um no-op mudo -- a checagem
	@# nunca cobria libflight_tc.so de verdade, so' fingia. Mesmo padrao ja
	@# corrigido em models/players/template/Makefile: varre TODO .so que de
	@# fato existe em $(DEST_DIR), sem lista hardcoded.
	@for so in $(DEST_DIR)/lib/mixr-plugins/*.so; do \
		[ -e "$$so" ] || continue; \
		ldd "$$so" | grep -q 'not found' && \
			{ echo "$(RED)install: dependencia nao resolvida em $$so$(NC)"; exit 1; } || true; \
	done
	@echo "$(GREEN)install: OK$(NC) -> $(DEST_DIR)/lib/mixr-plugins/, $(DEST_DIR)/share/mixr-plugins/flight/"

install-host: install ## Deposita em ../../../plugins/ -- nao toca dist/ do host.
	@mkdir -p $(ROOT)/plugins $(ROOT)/plugins/data
	@cp -a $(DEST_DIR)/lib/mixr-plugins/. $(ROOT)/plugins/
	@# NENHUM nome de modelo escrito aqui, de proposito -- mesmo padrao (e
	@# mesmo "porque") ja validado em models/players/template/Makefile.
	@#
	@# ACHADO POR AUDITORIA, CORRIGIDO (nao redescobrir): estas tres linhas
	@# cravavam o literal 'flight' ('mkdir -p .../plugins/data/flight',
	@# 'cp -a .../share/mixr-plugins/flight/.' e o 'rm -rf' correspondente
	@# no uninstall-host). Isso e' invisivel enquanto o modelo existe, mas
	@# torna a REMOCAO dele impossivel de automatizar: o nome do diretorio
	@# de dados nao e' derivavel do nome da PASTA (aqui a pasta e' 'A-4' e o
	@# diretorio e' 'flight'), entao um comando de remocao que so' tenha o
	@# nome da pasta nao tem como descobri-lo -- e o Makefile que sabia a
	@# resposta some junto com a pasta. Copiar o diretorio INTEIRO resolve
	@# para qualquer nome, sem ninguem precisar saber qual e', e e'
	@# exatamente o que o 'sync-plugins' da raiz ja faz um nivel acima.
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
	@# Os nomes saem do PROPRIO ./dist local, nunca escritos a mao -- so' o
	@# que ESTE modelo publicou e' removido, nunca o dado de um terceiro que
	@# divida o deposito. Ver o comentario do install-host acima para o
	@# porque de nao haver 'flight' cravado aqui.
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
