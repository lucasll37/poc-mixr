# ==============================================================================
# LOGICA COMPARTILHADA entre os Makefiles de models/players/<nome>/ -- NAO tem
# alvo default nem se basta sozinho, e' 'include'do, nunca chamado direto.
#
# Extraido depois de medir (nao por suspeita): ~65-70 linhas de texto NUCLEO
# identicas entre models/players/A-4/Makefile e models/players/template/
# Makefile, concentradas quase inteiramente no bloco de variaveis e no alvo
# 'configure' (o STALE-check + '--clearcache' + as tres armadilhas de
# pkg-config documentadas abaixo) -- e o comentario do 'configure' ja tinha
# comecado a divergir em REDACAO (nao em logica) entre os dois ANTES desta
# extracao, o sintoma exato que uma fonte unica evita.
#
# O QUE FICA DE FORA, de proposito -- genuinamente diferente entre modelos,
# nao cosmetica: build/test/install/install-host/uninstall-host (a lista de
# .so publicada e o diretorio de dados sao por-modelo) e qualquer alvo extra
# que so um modelo tenha (create-bt/update-bt/open-groot, hoje so em A-4).
# Cada Makefile-filho os declara por cima, DEPOIS do 'include' abaixo.
#
# CONTRATO com quem inclui este arquivo -- definir ANTES do 'include':
#   ROOT              obrigatorio. $(abspath ../../..) ou equivalente pra
#                      profundidade do destino -- e' esta linha, no Makefile-
#                      FILHO (nunca aqui), que 'scripts/models.sh::linha_root()'
#                      acha e ajusta pra 'make new-model' num destino de
#                      qualquer profundidade. Mover pra ca quebraria isso.
#   BUILD_TYPE ?=      default 'Debug', igual nos dois hoje.
#   TESTS ?=           default 'true'.
#   EXTRA_MESON_OPTS   opcional -- flags extras pra 'meson setup' (ex.:
#                      A-4 usa '-Dvariants=$(VARIANTS) -Dasan=$(ASAN)';
#                      template nao declara essas options no meson_options.txt
#                      dele, entao fica de fora, nao so vazio por preguica).
#   EXTRA_STALE_KEY    opcional -- entra na chave de cache de reconfiguracao
#                      (WANT=/.configure-args), pra um EXTRA_MESON_OPTS que
#                      mudou disparar reconfigure tambem.
#
# '.PHONY'/'help' continuam funcionando por construcao, sem mudanca nenhuma
# aqui: GNU Make ACUMULA $(MAKEFILE_LIST) (todo arquivo incluido, na ordem),
# e 'help:' ja fazia grep sobre ELE -- nao sobre "este arquivo" -- entao um
# alvo `## comentado` no Makefile-filho continua aparecendo no 'make help'.
# ==============================================================================

.PHONY: clean check-root configure help
.DEFAULT_GOAL := help

PWD        := $(shell pwd)
BUILD_DIR  := ./build
DEST_DIR   := $(PWD)/dist
PKG_PATH   := $(ROOT)/dist/lib/pkgconfig,$(ROOT)/build
NINJA_JOBS := $(shell nproc)

RED   := \033[0;31m
GREEN := \033[0;32m
NC    := \033[0m

EXTRA_MESON_OPTS ?=
EXTRA_STALE_KEY  ?=

clean: ## Remove ./build e ./dist (LOCAIS -- nao mexe no $(ROOT)/dist do host).
	rm -rf $(BUILD_DIR) $(DEST_DIR)

check-root: ## Confere que o projeto HOST ja publicou o SDK (pre-requisito, uma vez).
	@test -f $(ROOT)/build/conan_meson_native.ini || { \
		echo "$(RED)faltando $(ROOT)/build/conan_meson_native.ini$(NC)"; \
		echo "  rode 'cd $(ROOT) && make configure' primeiro (uma vez)."; exit 1; }
	@test -f $(ROOT)/dist/lib/pkgconfig/poc-mixr-sdk.pc || { \
		echo "$(RED)faltando o SDK de plugin em $(ROOT)/dist$(NC)"; \
		echo "  rode 'cd $(ROOT) && make sdk' primeiro (uma vez)."; exit 1; }
	@echo "$(GREEN)check-root: OK$(NC) -> SDK do host publicado em $(ROOT)/dist"

configure: check-root ## meson setup, isolado neste projeto (./build), consumindo o SDK/Conan do host por pkg-config.
	@mkdir -p $(BUILD_DIR)
	@# So reconfigura (caro: reavalia TODAS as dependencias, ~1-2s) se o
	@# build.ninja nao existe AINDA, se TESTS/EXTRA_STALE_KEY/BUILD_TYPE
	@# mudaram desde a ultima vez, OU se meson.build/meson_options.txt
	@# foram editados (STALE) -- 'meson compile'/'install', chamados
	@# depois, ja fazem seu proprio no-op rapido quando nada mudou; era o
	@# --reconfigure INCONDICIONAL (repetido a cada 'make run-*'/'test')
	@# que respondia pela maior parte do tempo de um "so rodar de novo".
	@# A checagem STALE existe porque o NINJA tem seu PROPRIO regen
	@# automatico (dispara sozinho quando meson.build muda, independente
	@# deste guard) e ele NAO repassa '-Dpkg_config_path' -- confirmado
	@# quebrando: editar meson.build e rodar 'make models' de novo, sem
	@# passar por aqui, falhava com "Dependency poc-mixr-sdk not found".
	@# O native-file do Conan entra no STALE junto com os meson.build: um
	@# 'make configure' na RAIZ reescreve conan_meson_native.ini, e o regen
	@# AUTOMATICO do ninja dispara por causa dele -- sem repassar
	@# '-Dpkg_config_path', exatamente como descrito acima. Confirmado
	@# quebrando: 'make configure' na raiz seguido de 'make test' falhava com
	@# "Dependency poc-mixr-sdk not found", com o .pc presente em dist/.
	@#
	@# O proprio poc-mixr-sdk.pc entra pelo MESMO motivo, e a falha e ainda
	@# mais silenciosa: o meson resolve o 'Libs:' do .pc em tempo de CONFIGURE
	@# e grava os caminhos das .so direto no build.ninja. Uma lib NOVA no SDK
	@# (o -lxinfer, por exemplo) nao aparece em lugar nenhum -- nem o guard nem
	@# o regen do ninja percebem -- e o link do plugin falha com "undefined
	@# reference" a uma funcao que existe e esta exportada. Confirmado
	@# quebrando, exatamente assim.
	@#
	@# E por isso que o ramo de reconfiguracao comeca com 'meson configure
	@# --clearcache': o meson CACHEIA o resultado do pkg-config entre
	@# execucoes, e '--reconfigure' sozinho reusa o cache -- medido, a
	@# dependencia continuava com as 4 .so antigas do SDK depois de um
	@# reconfigure completo. ('--clearcache' e argumento de 'meson configure',
	@# nao de 'meson setup' -- passa-lo ao setup faz o meson recusar a linha
	@# inteira, e a falha se parece com "nao mudou nada".)
	@#
	@# ACHADO POR AUDITORIA (nao redescobrir): o STALE abaixo so olhava o
	@# 'meson.build' de NIVEL TOPO -- editar um meson.build de SUBDIRETORIO
	@# (ex.: tests/meson.build) nao contava como STALE aqui, mas AINDA
	@# assim disparava o regen AUTOMATICO do proprio ninja (que 'meson
	@# compile' aciona sozinho quando ve QUALQUER meson.build mais novo que
	@# build.ninja, independente deste guard) -- e esse regen automatico e'
	@# exatamente o caminho que NAO repassa '-Dpkg_config_path' e cai no
	@# cache stale de pkg-config, do MESMO jeito que os dois paragrafos
	@# acima ja descrevem pro meson.build de topo. Reproduzido rodando:
	@# editar so' tests/meson.build e correr 'make models' (sem passar por
	@# este 'configure:' antes) falhava com "Dependency poc-mixr-sdk not
	@# found", igual ao caso ja documentado -- so' que este guard nao tinha
	@# pego. Por isso o STALE agora busca QUALQUER 'meson.build'/
	@# 'meson_options.txt' do projeto (recursivo, exceto dentro de
	@# ./build/), nao so' os de nivel topo.
	@#
	@# ARMADILHA MEDIDA (nao redescobrir), a MESMA ja registrada no 'configure:'
	@# do Makefile RAIZ: o ramo de reconfiguracao abaixo tambem e' o ramo do
	@# PRIMEIRO configure (quando nao ha build.ninja nenhum), e ali
	@# '--reconfigure' EXIGE um build tree do meson ja existente -- msetup.py
	@# testa build/meson-private/coredata.dat e, sem ele, aborta com "Directory
	@# does not contain a valid build tree". Ou seja: um 'make models' logo
	@# depois de um 'make clean' (build/ do modelo inexistente) morria aqui,
	@# antes de compilar nada -- medido no Meson 0.61.2 do Ubuntu 22.04. Por
	@# isso a flag e' condicional, e o teste e' pelo MESMO arquivo que o meson
	@# consulta, NAO pelo build.ninja do STALE-check acima: com coredata.dat
	@# presente a flag e' OBRIGATORIA (senao o meson so imprime "Directory
	@# already configured" e sai 0, sem reconfigurar nada), e um tree meio
	@# configurado (coredata.dat sem build.ninja, setup interrompido) so
	@# reconfigura direito por este teste.
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

help: ## Lista os alvos deste Makefile, com descricao (e' o que 'make' sem alvo roda).
	@# '-h' (nunca prefixar com o nome do arquivo) e' o que muda ao incluir
	@# este common.mk -- $(MAKEFILE_LIST) passa a ter MAIS de um arquivo
	@# (o Makefile-filho + este), e o grep multi-arquivo DEFAULT do POSIX
	@# prefixa cada linha com "<arquivo>:", empurrando o nome do ALVO pra
	@# fora da primeira coluna do awk. Sem TESTE nenhum antes de escrever
	@# isto teria passado direto -- achado rodando 'make help' de verdade
	@# apos a extracao.
	@grep -hE '^[a-zA-Z0-9_-]+:.*?## .*$$' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-16s\033[0m %s\n", $$1, $$2}'
