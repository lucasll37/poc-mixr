# ==============================================================================
# Logica compartilhada entre os Makefiles dos modelos (models/<categoria>/<nome>/).
# E' 'include'do, nunca chamado direto.
#
# AQUI: as variaveis comuns + clean/check-root/configure/check-organization/help
# + install/install-core/uninstall-core (ver PUBLISH_DATA abaixo).
# NO MAKEFILE-FILHO: build/test (dependem do meson.build de cada projeto) e
# create-bt/update-bt/open-groot (dependem de tools/dump-tree-model, que so
# existe num modelo que de fato tenha uma arvore de comportamento). O filho os
# declara DEPOIS do include.
#
# install/install-core/uninstall-core sao identicos entre os projetos de
# modelo, exceto por um detalhe: modelos com dado em configs:/data: publicam
# share/mixr-plugins/, modelos sem dado (missile, Beacon) nao tem nada a
# publicar. Esse grau de liberdade vira a variavel PUBLISH_DATA (default
# true), evitando duplicar o alvo por modelo.
#
# 'check-organization' fica AQUI (nao no filho) porque tools/check_organization.py
# e a MESMA copia, byte a byte, em todo modelo (ver o cabecalho do proprio
# script) -- nao ha nada por-modelo pra declarar. Nao precisa de
# 'configure'/'build': e analise estatica sobre o FONTE, sem compilar nada.
# Nome do ALVO em ingles, como todo alvo deste Makefile (build/test/install/
# check-root/create-bt/...) -- so' os COMENTARIOS/mensagens ficam em pt-BR, a
# convencao deste repositorio. Deliberadamente DIFERENTE de 'lint' -- esse
# nome ja e' de src/ui/scripts/edl_lint.py (lint ESTRUTURAL de arquivo .edl,
# conceito nao relacionado), e usa-lo aqui tambem confundiria os dois.
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
#   PUBLISH_DATA ?=   default 'true'. 'false' so' para um modelo sem nada em
#                     configs:/data: pra publicar (missile, Beacon hoje) --
#                     desliga a copia/remocao de share/mixr-plugins/ em
#                     install-core/uninstall-core.
#
# 'help' do filho continua funcionando: o GNU Make ACUMULA $(MAKEFILE_LIST), e o
# grep abaixo varre a lista inteira, nao so este arquivo.
# ==============================================================================

.PHONY: clean check-root configure check-organization help install install-core uninstall-core
.DEFAULT_GOAL := help

PWD        := $(shell pwd)
BUILD_DIR  := ./build
DEST_DIR   := $(PWD)/dist
PKG_PATH   := $(ROOT)/dist/lib/pkgconfig,$(ROOT)/build
NINJA_JOBS := $(shell nproc)

# Desliga o cache de bytecode do CPython (__pycache__/*.pyc) para os
# 'python3 tools/check_organization.py'/'conan'/etc. que os alvos deste
# arquivo (e do Makefile-filho) disparam. Repetido aqui, e nao so na raiz,
# porque cada projeto de modelo e' AUTOCONTIDO por design -- 'cd
# models/<nome> && make' roda sem o Makefile raiz no meio (ver o cabecalho
# deste arquivo), entao o 'export' de la nunca chega aqui nesse caminho.
export PYTHONDONTWRITEBYTECODE := 1

RED   := \033[0;31m
GREEN := \033[0;32m
NC    := \033[0m

EXTRA_MESON_OPTS ?=
EXTRA_STALE_KEY  ?=
PUBLISH_DATA     ?= true

clean: ## Remove ./build e ./dist LOCAIS -- nao mexe no dist/ do core.
	rm -rf $(BUILD_DIR) $(DEST_DIR)

check-root: ## Confere que o core ja publicou o SDK (pre-requisito, uma vez).
	@test -f $(ROOT)/build/conan_meson_native.ini || { \
		echo "$(RED)faltando $(ROOT)/build/conan_meson_native.ini$(NC)"; \
		echo "  rode 'cd $(ROOT) && make configure' primeiro (uma vez)."; exit 1; }
	@test -f $(ROOT)/dist/lib/pkgconfig/poc-mixr-sdk.pc || { \
		echo "$(RED)faltando o SDK de plugin em $(ROOT)/dist$(NC)"; \
		echo "  rode 'cd $(ROOT) && make sdk' primeiro (uma vez)."; exit 1; }
	@echo "$(GREEN)check-root: OK$(NC) -> SDK do core publicado em $(ROOT)/dist"

configure: check-root ## meson setup isolado em ./build, consumindo o SDK do core.
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

check-organization: ## Linter OPCIONAL de organizacao interna (tools/check_organization.py) -- nao bloqueia build/test/install.
	@test -f tools/check_organization.py || { \
		echo "$(RED)faltando tools/check_organization.py$(NC)"; \
		echo "  copie de models/template/tools/check_organization.py (ver o cabecalho do proprio arquivo)."; exit 1; }
	@python3 tools/check_organization.py

install: build ## Instala em ./dist deste projeto (lib/ + share/mixr-plugins/, se PUBLISH_DATA).
	@# '--only-changed': sem isto o 'meson install' recopia com mtime NOVO mesmo
	@# com conteudo identico, e um destino mais novo que um input ja linkado (ex.:
	@# dist/lib/libxtrack.so, do SDK) faz o ninja RELINKAR na proxima chamada.
	meson install -C $(BUILD_DIR) --no-rebuild --only-changed
	@# Varre todo .so que de fato existe, sem nome cravado: depois de renomear o
	@# projeto o .so deixa de se chamar lib<nome-antigo>.so, e um 'ldd' contra um
	@# caminho inexistente nao contem "not found" -- o '|| true' absorveria o
	@# erro e a linha seguinte imprimiria "install: OK" apontando pra nada.
	@for so in $(DEST_DIR)/lib/mixr-plugins/*.so; do \
		[ -e "$$so" ] || continue; \
		ldd "$$so" | grep -q 'not found' && \
			{ echo "$(RED)install: dependencia nao resolvida em $$so$(NC)"; exit 1; } || true; \
	done
	@if [ "$(PUBLISH_DATA)" = "true" ]; then \
	   echo "$(GREEN)install: OK$(NC) -> $(DEST_DIR)/lib/mixr-plugins/, $(DEST_DIR)/share/mixr-plugins/"; \
	 else \
	   echo "$(GREEN)install: OK$(NC) -> $(DEST_DIR)/lib/mixr-plugins/"; \
	 fi

install-core: install ## Deposita em $(ROOT)/plugins/ -- nao toca dist/ do core.
	@mkdir -p $(ROOT)/plugins
	@cp -a $(DEST_DIR)/lib/mixr-plugins/. $(ROOT)/plugins/
	@# A arvore vai JUNTO quando o modelo publica dado (PUBLISH_DATA=true): o
	@# 'treeFile:' do behavior deste modelo, quando existe, aponta pra ca, e os
	@# nos que ela referencia so existem dentro deste .so. Copia o diretorio
	@# INTEIRO, sem nome cravado -- o install_data do meson.build escreve em
	@# share/mixr-plugins/<nome-do-projeto>/, que este Makefile nao precisa saber.
	@if [ "$(PUBLISH_DATA)" = "true" ]; then \
	   mkdir -p $(ROOT)/plugins/data; \
	   if [ -d $(DEST_DIR)/share/mixr-plugins ]; then \
	      cp -a $(DEST_DIR)/share/mixr-plugins/. $(ROOT)/plugins/data/; \
	   fi; \
	   echo "$(GREEN)install-core: OK$(NC) -> $(ROOT)/plugins/, $(ROOT)/plugins/data/"; \
	 else \
	   echo "$(GREEN)install-core: OK$(NC) -> $(ROOT)/plugins/"; \
	 fi
	@echo "  (dist/ so e populado por 'cd $(ROOT) && make install')"

uninstall-core: ## Remove de $(ROOT)/plugins/ o que ESTE modelo publicou.
	@if [ -d $(DEST_DIR)/lib/mixr-plugins ]; then \
	   for so in $(DEST_DIR)/lib/mixr-plugins/*.so; do \
	      [ -e "$$so" ] || continue; \
	      rm -f "$(ROOT)/plugins/$$(basename $$so)"; \
	   done; \
	 fi
	@# Os nomes saem do PROPRIO ./dist local -- so o que ESTE modelo publicou e
	@# removido, nunca o de um terceiro que divida o deposito.
	@if [ "$(PUBLISH_DATA)" = "true" ] && [ -d $(DEST_DIR)/share/mixr-plugins ]; then \
	   for d in $(DEST_DIR)/share/mixr-plugins/*/; do \
	      [ -d "$$d" ] || continue; \
	      rm -rf "$(ROOT)/plugins/data/$$(basename $$d)"; \
	   done; \
	 fi

help: ## Lista os alvos deste Makefile (e' o que 'make' sem alvo roda).
	@# 'grep -h': $(MAKEFILE_LIST) tem mais de um arquivo (o Makefile-filho +
	@# este), e o grep multi-arquivo padrao prefixaria cada linha com
	@# "<arquivo>:", empurrando o nome do ALVO pra fora da primeira coluna.
	@grep -hE '^[a-zA-Z0-9_-]+:.*?## .*$$' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-16s\033[0m %s\n", $$1, $$2}'
