.PHONY: build test
.DEFAULT_GOAL := help

# ==============================================================================
# Makefile AUTOCONTIDO de Beacon.
#
# Abra o VS Code AQUI, rode `make`, e o .so sai em ./dist -- a raiz DESTE
# projeto, nao a do poc-mixr. Nao chama o Makefile raiz, e o alvo `models` dela
# tambem nao constroi este diretorio a menos que este projeto exista sob
# models/<categoria>/ (ver o cabecalho do meson.build).
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

PUBLISH_DATA := false  # este modelo nao publica configs:/data: (ver models/common.mk)

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
