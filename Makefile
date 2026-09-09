.PHONY: clean configure sdk models sync-plugins build install package help test-models run-app run-app-monitor run-node run-node-monitor venv-rl test-rl venv-rl-training test test-asan test-ci clean-ci docs open-docs open-presentation open-edl-builder open-groot new-model rm-model

.DEFAULT_GOAL := help

# Custom variables
PWD := $(shell pwd)
BUILD_DIR := ./build
DEST_DIR := $(PWD)/dist

# Deposito COMPARTILHADO dos modelos (flight/template, construidos por
# este repositorio, MAIS qualquer .so de terceiro -- ver
# plugins/README.md). 'make models' so escreve ATE aqui; dist/ e
# populado so por 'make install' (alvo 'sync-plugins'), do HOST.
PLUGINS_DIR := $(PWD)/plugins

# Number of parallel jobs for Ninja (all available cores)
NINJA_JOBS := $(shell nproc)

# Build configuration
BUILD_TYPE := Debug
# ASAN=true reconfigura o projeto do modelo com o sanitizador (ver test-asan).
ASAN ?= false

# NAME=/PLAYER=/SCENARIO=/ARGS=/CATEGORY= (new-model, run-app-monitor,
# run-node, run-node-monitor) sao valores passados pelo USUARIO na linha de comando.
# CORRIGIDO (nao redescobrir): as receitas desses alvos costumavam
# interpolar "$(NAME)" etc. -- substituicao de TEXTO do proprio Make, ANTES
# do shell ver a linha -- direto no meio de uma linha de shell. Uma aspa
# dupla no valor fecha a citacao mais cedo e o resto vira comando de shell
# adicional; confirmado explorando de verdade (make new-model
# NAME='x"; touch /tmp/PWNED; echo "y' criava o arquivo). Um 'case'/'test'
# de validacao ADICIONADO na mesma receita NAO protege -- a MESMA
# substituicao de texto acontece nessa linha tambem, entao o comando
# injetado roda antes mesmo do guard ser avaliado (medido).
# A correcao de verdade e nunca deixar o Make substituir o valor bruto
# dentro de uma linha de shell: 'export' bota o valor no AMBIENTE do
# processo filho, e a receita le com '$$NOME' (variavel de shell, expandida
# em runtime) em vez de '$(NOME)' (texto do Make) -- os metacaracteres
# ficam so' dentro do VALOR da variavel, nunca voltam a ser sintaxe de
# shell. Testado nos dois sentidos: o mesmo payload malicioso vira
# argumento literal inerte, e ARGS continua dividindo em varias palavras
# normalmente (word-splitting do shell sozinho, sem reabrir metacaracteres).
#
# LIMITE CONHECIDO, NAO FECHAVEL DAQUI (achado por autorevisao desta sessao,
# nao redescobrir tentando "consertar" de novo): o fix acima fecha
# metacaracteres de SHELL (';'/'&&'/backtick), mas nao fecha a sintaxe de
# FUNCAO do proprio Make, '$(shell ...)'. 'make new-model
# NAME=$(shell touch /tmp/x)' ainda executa o touch -- confirmado rodando,
# inclusive num Makefile de teste minimo SEM nenhuma mencao a NOME nenhum e
# SEM 'export'. O motivo e estrutural, nao um bug deste arquivo: o GNU Make
# expande ($(shell ...), $(wildcard ...), etc.) o valor de uma atribuicao
# de VARIAVEL DA LINHA DE COMANDO ao fazer o PARSE do argv, antes de ler
# qualquer regra deste Makefile -- nao ha gancho de Makefile que rode ANTES
# disso pra validar/rejeitar. Por isso 'make help NOME=$(shell touch /tmp/x)'
# (um alvo sem relacao NENHUMA com NOME) tambem executa -- o raio de
# alcance e QUALQUER invocacao de make neste repositorio, nao so os quatro
# alvos que usam NAME/PLAYER/SCENARIO/ARGS. Mitigar isso exigiria nao expor
# NENHUMA variavel de linha de comando (perderia a ergonomia de
# 'make alvo VAR=valor', o padrao do Makefile inteiro) ou envolver 'make'
# num wrapper que sanitize argv ANTES de invoca-lo -- nenhum dos dois feito
# aqui. Escopo de exploracao real: quem controla o ARGV de uma chamada de
# make (typado a mao, ou colado de uma fonte nao confiavel sem olhar) --
# ninguem programatico deste repositorio (CI, scripts) passa entrada
# externa direto pra cá (conferido na mesma auditoria). Risco aceito e
# documentado, nao "corrigido".
# ACHADO RODANDO, CORRIGIDO (nao redescobrir): estas atribuicoes eram '?=',
# e '?=' quer dizer "so' se ainda NAO estiver definida" -- e uma variavel de
# AMBIENTE conta como definida. O VS Code exporta 'NAME=Code' no ambiente do
# terminal integrado, entao 'make new-model CATEGORY=others' (sem NAME=)
# passava no 'test -n "$$NAME"' e criava models/others/Code -- reproduzido,
# a pasta foi criada de verdade. Vale para toda variavel de nome generico
# aqui (NAME/ARGS/SCENARIO/...), em qualquer ambiente que ja as exporte.
#
# ':=' e' a correcao certa e nao custa ergonomia nenhuma: pela precedencia do
# GNU Make, uma atribuicao no Makefile VENCE o ambiente, e a linha de comando
# vence a atribuicao no Makefile -- que e' exatamente a semantica desejada
# ('make alvo VAR=valor' continua funcionando; o ambiente para de vazar).
NAME :=
PLAYER :=
SCENARIO :=
ARGS :=
CATEGORY :=
SO :=
DATA :=
FORCE :=
DRY_RUN :=
export NAME PLAYER SCENARIO ARGS CATEGORY SO DATA FORCE DRY_RUN

# Colors for output
RED := \033[0;31m
GREEN := \033[0;32m
YELLOW := \033[1;33m
NC := \033[0m # No Color

# ============================================
# C++ Build Targets
# ============================================

clean: ## Remove build/, dist/ e plugins/ (host + todos os modelos).
	rm -rf $(BUILD_DIR)/
	rm -rf $(DEST_DIR)/
	rm -rf ./subprojects/packagecache
	@# Descoberto por find (mesma lista de MODELOS_PRODUCAO, ver alvo 'models'
	@# abaixo), + template (nunca entra em 'models:', mas alguem pode ter
	@# rodado 'make'/'make install-host' nele a mao enquanto experimentava --
	@# ver models/players/template/README.md). 'uninstall-host' roda ANTES do
	@# 'clean' de cada um: ele precisa do ./dist LOCAL ainda intacto para
	@# saber quais basenames remover de plugins/ -- 'clean' apaga esse ./dist
	@# logo em seguida. '|| true' nos dois porque um clean antes do primeiro
	@# 'make models' nao tem nada para limpar/desinstalar ali. So os nomes
	@# que CADA modelo de fato publicou sao removidos -- um .so de TERCEIRO
	@# com outro nome (ver plugins/README.md) nunca e tocado.
	@for d in $(MODELOS_PRODUCAO) models/players/template; do \
	   $(MAKE) -C $$d uninstall-host 2>/dev/null || true; \
	   $(MAKE) -C $$d clean 2>/dev/null || true; \
	 done
	@# ACHADO POR AUDITORIA, CORRIGIDO (nao redescobrir): aqui havia um
	@# '@rm -rf $(PLUGINS_DIR)/data' INCONDICIONAL -- o namespace de dados
	@# INTEIRO, sem filtro de nome. Isso contradizia, no codigo, a garantia
	@# que plugins/README.md promete por escrito ("make clean so remove os
	@# nomes que ESTE repositorio gera -- um .so de terceiro com outro nome
	@# nao e apagado por engano"): a metade '.so' da promessa era verdadeira,
	@# a metade 'data/' era um rm -rf de pasta compartilhada. O laco acima ja
	@# removeu, por NOME, o que cada modelo publicou (o 'uninstall-host' de
	@# cada um deriva os nomes do proprio ./dist local, sem literal cravado).
	@# O que sobra aqui e' so' recolher o diretorio se ele tiver ficado
	@# VAZIO -- 'rmdir' falha sozinho, e sem estrago, se houver qualquer
	@# coisa dentro (o caso do dado de um terceiro).
	@rmdir $(PLUGINS_DIR)/data 2>/dev/null || true

configure: ## Configura o projeto para build (conan install + meson setup).
	mkdir -p $(BUILD_DIR)/
	conan install ./ \
		--build=missing \
		--settings=build_type=$(BUILD_TYPE) \
		-c tools.system.package_manager:mode=install \
		-c tools.system.package_manager:sudo=True

	# O meson DESCARTA o PKG_CONFIG_PATH do ambiente quando o native-file do Conan
	# fixa 'pkg_config_path' (medido). Tem de ir por linha de comando -- e o
	# separador de lista do meson e VIRGULA, nao dois-pontos (relevante quando um
	# modelo/plugin soma o proprio dist/lib/pkgconfig a este caminho).
	@# ARMADILHA MEDIDA (nao redescobrir): '--reconfigure' EXIGE um build tree
	@# do meson ja existente -- msetup.py testa build/meson-private/coredata.dat
	@# e, se nao achar, aborta com "Directory does not contain a valid build
	@# tree". Na PRIMEIRA configure depois de um 'make clean' o build/ so tem a
	@# saida do conan (os .pc + conan_meson_native.ini), nunca um tree do meson,
	@# e o alvo morria ali (medido no Meson 0.61.2 do Ubuntu 22.04). Por isso a
	@# flag entra so quando o tree ja existe -- e o teste e' pelo MESMO arquivo
	@# que o meson consulta, nao por build.ninja: com coredata.dat presente a
	@# flag e' OBRIGATORIA, senao o meson so imprime "Directory already
	@# configured" e sai 0, sem reconfigurar nada.
	if [ -f $(BUILD_DIR)/meson-private/coredata.dat ]; \
		then RECONF=--reconfigure; else RECONF=; fi; \
	meson setup $$RECONF \
		--backend ninja \
		--buildtype $(shell echo $(BUILD_TYPE) | tr '[:upper:]' '[:lower:]') \
		--native-file $(BUILD_DIR)/conan_meson_native.ini \
		--prefix=$(DEST_DIR) \
		--libdir=$(DEST_DIR)/lib \
		-Dpkg_config_path=$(BUILD_DIR) \
		$(BUILD_DIR)/ .


sdk: ## Publica o SDK de plugin em dist/ -- etapa PREVIA ao build do modelo.
	@# 'meson compile' com os NOMES dos alvos. Medido: o meson compile resolve
	@# por NOME e traduz para o caminho de saida; o ninja cru resolve so por
	@# CAMINHO ('ninja xboard' -> "unknown target"). Usar o meson aqui evita
	@# ter de escrever libs/xboard/libxboard.so a mao.
	meson compile -C $(BUILD_DIR) xboard xlog xtrack xrlbridge xinfer xpyembed events
	@# '--tags sdk,devel' e nao '--tags sdk': install_headers() nao aceita
	@# install_tag no meson 1.2, entao os headers ficam com a tag automatica
	@# 'devel'. Com '--tags sdk' sozinho o SDK sai SEM os headers, e o erro so
	@# aparece dois alvos depois, como "PluginAbi.hpp: No such file or directory".
	@# '--only-changed': sem isto, cada 'make sdk' RECOPIA libxboard/libxlog/
	@# libxtrack pra dist/lib/ com mtime NOVO mesmo sem mudanca de conteudo --
	@# e um destino mais novo que um input ja linkado faz o ninja dos modelos
	@# (flight/template, que dependem do SDK) achar que precisa RELINKAR
	@# na proxima chamada. Confirmado com 'ninja -C models/players/A-4/build -d
	@# explain libflight.so'. Sem isto, TODO 'make install'/'run-*'/'test'
	@# relinkava os quatro plugins de producao, mesmo com tudo ja compilado.
	meson install -C $(BUILD_DIR) --no-rebuild --tags sdk,devel --only-changed
	@test -f $(DEST_DIR)/lib/pkgconfig/poc-mixr-sdk.pc || { echo "$(RED)sdk: .pc ausente$(NC)"; exit 1; }
	@test -f $(DEST_DIR)/include/xplugin/PluginAbi.hpp || { echo "$(RED)sdk: headers ausentes -- use --tags sdk,devel$(NC)"; exit 1; }
	@echo "$(GREEN)sdk: OK$(NC) -> dist/include, dist/lib, dist/lib/pkgconfig"

# ==============================================================================
# 'models' e 'sync-plugins' -- DECOPLADOS de proposito.
#
# 'models' compila e instala TODO modelo de producao sob models/ --
# QUALQUER subpasta dela (players/, e qualquer outra que vier a existir,
# ex.: others/, events/, systems/ -- hoje so 'events/' tem conteudo, e e
# a excecao conhecida: ver abaixo), cada um projeto Meson AUTOCONTIDO (ver
# models/README.md §1.1) -- DESCOBERTO POR FIND (nao por lista fixa: um
# modelo novo so precisa existir, em QUALQUER subpasta de models/, nunca
# precisa de uma linha nova aqui -- mesma filosofia ja usada por
# tests/guard/check_modelo_estrutura.sh; check_colisao_fabrica.py e
# deliberadamente mais estreito, so models/players/ -- ver o cabecalho
# dele),
# delegando pro Makefile de CADA um (`$(MAKE) -C models/players/<nome>
# install-host TESTS=true VARIANTS=true ASAN=...`), nao reimplementando o
# setup aqui. Exclui models/players/template/ de MODELOS_PRODUCAO (nunca e
# producao) mas instala o SEGUNDO artefato dele (libtemplate_mirror.so) a
# parte, logo abaixo -- ele precisa estar em plugins/ para os testes de
# plugin do host que o carregam (herdou esse papel de
# models/players/fixtures/stub, removido). O resultado -- so os .so, flat,
# mais os dados de cada um que publicar (hoje, so o flight/A-4) -- pousa em
# plugins/ (e plugins/data/<nome>/), o MESMO deposito que um terceiro
# usaria (ver plugins/README.md). Este alvo NUNCA escreve em dist/ -- e por
# isso "desacoplado do restante": compilar/instalar um modelo nao presume
# nada sobre onde o HOST guarda os artefatos dele.
#
# Sem mecanismo de CI neste repositorio (nao ha pipeline nenhum rodando em
# lugar nenhum) -- so este Makefile e quem "orquestra". Por isso as flags
# de build completo (TESTS/VARIANTS/ASAN) vao direto na chamada abaixo, nao
# atras de um alvo `install-host-ci` a parte: um modelo que nao usa alguma
# delas (template nao tem `variants`) so a ignora, o GNU Make nao
# reclama de variavel de linha de comando nao consumida.
#
# 'sync-plugins' e a UNICA ponte para dist/ -- copia plugins/*.so
# (de QUALQUER origem: os modelos deste repositorio OU terceiro, ja
# indistinguiveis neste ponto) para dist/lib/mixr-plugins/, e plugins/data/
# para dist/share/mixr-plugins/. So roda como parte de 'install' -- e por
# isso "so no install do restante do projeto": nem 'build' nem 'models'
# tocam dist/lib/mixr-plugins/ sozinhos.
# ==============================================================================

# MESMA logica de descoberta de tests/guard/check_modelo_estrutura.sh (todo
# diretorio sob models/ -- QUALQUER subpasta, nao so players/ -- com um
# meson.build de PROJETO), e pelo MESMO motivo: um projeto de verdade
# declara project() na raiz; um meson.build de subdiretorio auxiliar
# (tests/, tools/) so tem subdir(), e um meson.build de CONTRATO/SDK
# consumido por subdir() do host (models/events/, hoje a UNICA excecao
# real -- ver models/events/README.md: "nao e um modelo em si") tambem
# nunca declara project(). E por isso que o grep por '^project' abaixo
# já FILTRA sozinho qualquer subpasta de models/ que nao seja um modelo de
# verdade -- 'others/'/'systems/' (hoje vazias, so com .gitkeep) entram
# nessa varredura sem exigir nenhuma linha nova aqui assim que ganharem um
# projeto de verdade dentro. A UNICA exclusao por PATH, alem das de
# build/dist/subprojects (artefatos de build, nunca fonte) e tests/tools/
# (subdir() auxiliar, sem Makefile proprio -- ver models/players/A-4/tools/,
# o gerador dump-tree-model), e template/: ele TEM project() (e um
# projeto de verdade, compila e testa sozinho) mas nunca e producao (ver
# models/players/template/README.md) -- por isso segue excluido por path,
# nao pelo filtro de project().
# O grep abaixo casa so 'project' (sem o '(' de 'project(' que
# check_modelo_estrutura.sh usa) de proposito -- um '(' sozinho, colado
# direto numa string dentro de um '$(shell ...)', desbalanceia a contagem de
# parenteses que o PROPRIO parser do GNU Make faz para achar o fim da
# chamada (ele conta caracteres literalmente, sem entender aspas de shell):
# o sintoma medido foi "unterminated call to function 'shell': missing )".
# Sem o '(', o casamento continua inequivoco -- nenhum meson.build deste
# repositorio comeca uma linha com 'project' fora da propria declaracao
# project(...).
MODELOS_PRODUCAO := $(shell find models -mindepth 2 -name meson.build \
                       -not -path '*/build/*' -not -path '*/dist/*' -not -path '*/subprojects/*' \
                       -not -path '*/template/*' -not -path '*/tests/*' -not -path '*/tools/*' \
                       -exec grep -q '^project' {} \; -print \
                     | xargs -r -n1 dirname | sort -u)

models: sdk ## Compila os modelos e deposita em plugins/ -- nao toca dist/ (ver 'install').
	@for d in $(MODELOS_PRODUCAO); do \
	   $(MAKE) -C $$d install-host TESTS=true VARIANTS=true ASAN=$(ASAN) || exit 1; \
	 done
	@# template/ nunca e producao (ver models/players/template/README.md) mas
	@# o SEGUNDO artefato dele (libtemplate_mirror.so) precisa estar em
	@# plugins/ para os testes de plugin do host (tests/meson.build,
	@# plugin-modelo-estranho/plugin-deposito-terceiro) -- mesmo papel que
	@# fixtures/stub tinha antes de ser removido.
	@$(MAKE) -C models/players/template install-host TESTS=true ASAN=$(ASAN) || exit 1
	@echo "$(GREEN)models: OK$(NC) -> $(PLUGINS_DIR)/ ($(words $(MODELOS_PRODUCAO)) projeto(s) de producao: $(notdir $(MODELOS_PRODUCAO)); + template/; rode 'make install' para sincronizar com dist/)"

sync-plugins: ## Copia plugins/ -> dist/ -- so aqui um cenario enxerga o modelo.
	@# plugins/ ja mistura o que os modelos locais depositaram (via
	@# 'models', acima) com qualquer .so de terceiro (ver
	@# plugins/README.md) -- dali em diante os dois sao INDISTINGUIVEIS,
	@# e essa e a ideia: o mesmo passo de copia cobre os dois casos. Pasta
	@# vazia (build limpo, nada depositado) e um no-op silencioso, nao erro.
	@#
	@# ESPELHO, nao copia acumulativa: alem de copiar, PODA de dist/ o que
	@# nao existe mais em plugins/. O comentario do bloco de cabecalho deste
	@# arquivo ja afirma que 'sync-plugins' e' a UNICA ponte para dist/ --
	@# auditado e confirmado (os unicos install_dir para
	@# lib/share/mixr-plugins sao os dos meson.build dos modelos, e o prefix
	@# deles e' o ./dist LOCAL de cada um). Com a copia sem poda, dist/ so'
	@# CRESCIA: um .so de um modelo removido ficava la para sempre e era
	@# RECOPIADO a cada 'make install' (medido). Agora dist/ e' estado
	@# DERIVADO de plugins/, e qualquer remocao -- inclusive um 'rm -rf' da
	@# pasta do modelo, feito por quem nunca vai usar 'make rm-model' --
	@# se auto-cura aqui.
	@#
	@# A poda fica DENTRO do ramo de deposito nao-vazio, de proposito:
	@# deposito vazio significa "modelos ainda nao construidos" (o mesmo
	@# estado que o 'else' abaixo ja trata como aviso amarelo), e podar ali
	@# esvaziaria dist/ num 'configure && build && install' sem
	@# 'make models' -- 'install' NAO depende de 'models', de proposito.
	@#
	@# A comparacao e' sempre plugins/ contra dist/, NUNCA contra uma lista
	@# de nomes "que este repo gera": tests/plugin/run_thirdparty_deposit.py
	@# deposita nos dois lados e limpa num 'finally' que NAO roda sob
	@# SIGTERM (o kill do timeout de 'meson test') -- uma allowlist brigaria
	@# com esse leftover; a comparacao posicional convive com ele.
	mkdir -p $(DEST_DIR)/lib/mixr-plugins/
	@if ls $(PLUGINS_DIR)/*.so >/dev/null 2>&1; then \
	   cp -v $(PLUGINS_DIR)/*.so $(DEST_DIR)/lib/mixr-plugins/; \
	   for so in $(PLUGINS_DIR)/*.so; do \
	      base=$$(basename "$$so"); \
	      ldd $(DEST_DIR)/lib/mixr-plugins/$$base | grep -q 'not found' && { echo "$(RED)sync-plugins: $$base com dependencia nao resolvida$(NC)"; exit 1; } || true; \
	   done; \
	   for so in $(DEST_DIR)/lib/mixr-plugins/*.so; do \
	      [ -e "$$so" ] || continue; \
	      base=$$(basename "$$so"); \
	      [ -e "$(PLUGINS_DIR)/$$base" ] || { \
	         rm -f "$$so"; \
	         echo "$(YELLOW)sync-plugins: podado dist/lib/mixr-plugins/$$base (nao existe mais em $(PLUGINS_DIR)/)$(NC)"; }; \
	   done; \
	 else \
	   echo "$(YELLOW)sync-plugins: aviso: $(PLUGINS_DIR) nao tem nenhum .so -- se voce esperava um modelo carregar,$(NC)"; \
	   echo "$(YELLOW)  rode 'make models' antes de 'make install' (as duas sao DECOPLADAS de proposito, ver CLAUDE.md).$(NC)"; \
	 fi
	@# Dados (hoje, so o flight: jsbsim/ + flight_tree.xml) -- unica excecao
	@# ao deposito flat de plugins/, documentada em
	@# plugins/README.md.
	@if [ -d $(PLUGINS_DIR)/data ]; then \
	   mkdir -p $(DEST_DIR)/share/mixr-plugins/; \
	   cp -a $(PLUGINS_DIR)/data/. $(DEST_DIR)/share/mixr-plugins/; \
	   for d in $(DEST_DIR)/share/mixr-plugins/*/; do \
	      [ -d "$$d" ] || continue; \
	      base=$$(basename "$$d"); \
	      [ -d "$(PLUGINS_DIR)/data/$$base" ] || { \
	         rm -rf "$$d"; \
	         echo "$(YELLOW)sync-plugins: podado dist/share/mixr-plugins/$$base/ (nao existe mais em $(PLUGINS_DIR)/data/)$(NC)"; }; \
	   done; \
	 fi
	@# Aviso (NUNCA erro) sobre .so em plugins/ que nenhum modelo vivo
	@# reclama. E' o ponto de deteccao mais cedo possivel para o orfao que
	@# sobra de um modelo apagado a mao -- todo 'make install', nao so'
	@# 'make test'. E' AVISO e nao falha porque "nao reivindicado" inclui,
	@# legitimamente, um .so de TERCEIRO: plugins/ mistura os dois de
	@# proposito e eles sao indistinguiveis a partir dali
	@# (plugins/README.md; tests/plugin/run_thirdparty_deposit.py prova).
	@reclamados=""; \
	 for d in $(MODELOS_PRODUCAO) models/players/template; do \
	    for so in $$d/dist/lib/mixr-plugins/*.so; do \
	       [ -e "$$so" ] && reclamados="$$reclamados $$(basename $$so)"; \
	    done; \
	 done; \
	 for so in $(PLUGINS_DIR)/*.so; do \
	    [ -e "$$so" ] || continue; \
	    base=$$(basename "$$so"); \
	    case " $$reclamados " in *" $$base "*) ;; *) \
	       echo "$(YELLOW)sync-plugins: aviso: $$base nao e reclamado por nenhum modelo de models/$(NC)"; \
	       echo "$(YELLOW)  (ok se for .so de terceiro; se sobrou de um modelo apagado: make rm-model SO=$$base)$(NC)";; \
	    esac; \
	 done
	@echo "$(GREEN)sync-plugins: OK$(NC) -> $(DEST_DIR)/lib/mixr-plugins/, $(DEST_DIR)/share/mixr-plugins/"

# ============================================
# Scaffold de modelo novo
# ============================================

new-model: ## Copia template/ num modelo novo. Uso: NAME= CATEGORY=player|system|others.
	@test -n "$$NAME" || { echo "$(RED)uso: make new-model NAME=meu_modelo CATEGORY=player|system|others$(NC)"; exit 1; }
	@test -n "$$CATEGORY" || { echo "$(RED)uso: make new-model NAME=meu_modelo CATEGORY=player|system|others$(NC)"; exit 1; }
	scripts/models.sh --name "$$NAME" --category "$$CATEGORY"

# NAME/CATEGORY/SO/DATA/FORCE/DRY_RUN lidos com '$$X' (variavel de SHELL, via
# o 'export' la em cima), NUNCA '$(X)' -- ver a armadilha de injecao
# documentada no cabecalho deste arquivo.
#
# Dois modos, e a diferenca importa: com NAME a pasta tem de existir (e' dela
# que saem os nomes dos artefatos); com SO voce remove um artefato orfao cuja
# fonte ja nao existe mais. O script recusa se algum cenario ainda referenciar
# o modelo -- e' isso que impede a remocao de GERAR a inconsistencia.
rm-model: ## Remove um modelo. Uso: NAME= CATEGORY= | SO=libX.so [DATA=dir] [FORCE=1] [DRY_RUN=1].
	@test -n "$$NAME" -o -n "$$SO" || { \
	   echo "$(RED)uso: make rm-model NAME=meu_modelo CATEGORY=player|system|others$(NC)"; \
	   echo "$(RED)  ou: make rm-model SO=libx9.so [DATA=x9]   (orfao legado, sem fonte)$(NC)"; \
	   exit 1; }
	@scripts/models.sh --remove \
	   $${NAME:+--name "$$NAME"} $${CATEGORY:+--category "$$CATEGORY"} \
	   $${SO:+--so "$$SO"} $${DATA:+--data "$$DATA"} \
	   $${FORCE:+--force} $${DRY_RUN:+--dry-run}

# ============================================
# Build / Install / Package do HOST
# ============================================

build: sdk ## Compila os executaveis do HOST -- nao precisa dos modelos.
	meson compile -C $(BUILD_DIR) -j$(NINJA_JOBS)

install: build sync-plugins ## Instala o host em dist/bin/ e sincroniza plugins/ -> dist/.
	@# '--only-changed' -- mesmo "porque" do alvo 'sdk' acima: evita mtime
	@# novo em dist/bin/ sem necessidade a cada chamada.
	meson install -C $(BUILD_DIR) --only-changed

package: ## Gera o pacote Conan deste projeto.
	conan create ./ \
		--build=missing \
		--settings=build_type=$(BUILD_TYPE)

# ============================================
# Execution Targets
# ============================================

# Alvos de check/run por poc ou cenario PARTICULAR foram removidos daqui --
# use './app -folder <pasta> -scenario <nome>'/'-f <arquivo>' diretamente (nao
# ha mais catalogo estatico -- qualquer poc com um unico .edl(.in) em
# configs/ ja e alcancavel) e
# 'tests/determinism/check_determinism.sh <binario> <rotulo> <frames>
# [fixture-poc] [arquivo-de-cenario]' para determinismo. 'make install'
# continua sendo o pre-requisito (dlopen do modelo so em tempo de execucao).
#
# 'check-plugin-hotswap' tambem saiu (junto com
# tests/plugin/check_hotswap_rebuild.sh) -- NAO reintroduzir: a propriedade
# que ele media ja e afirmada por 'plugin-hotswap' (suite 'plugin', dentro de
# 'make test'), com a MESMA fixture e a MESMA comparacao de hdg do falcon1; e
# o "rebuildar so o .so nao toca o executavel" e verdadeiro por CONSTRUCAO --
# host e modelo sao projetos meson separados, em arvores de build separadas
# ('build/' x 'models/<...>/build/'), sem aresta possivel entre eles. Em
# troca, o alvo editava fonte VERSIONADO com 'sed -i' (restaurado so por um
# 'trap EXIT') e sobrescrevia dist/lib/mixr-plugins/libflight.so a mao.

run-app: install ## Roda dist/bin/app (TUI) sobre ./sandbox.
	$(DEST_DIR)/bin/app -folder ./sandbox

run-app-monitor: install ## Roda o app com o Monitor do Groot. Uso: PLAYER=falcon1 [ARGS=...].
	@test -n "$$PLAYER" || { echo "$(RED)uso: make run-app-monitor PLAYER=<nome-do-player>";  exit 1; }
	MIXR_GROOT_MONITOR="$$PLAYER" $(DEST_DIR)/bin/app $${ARGS:--folder ./sandbox}

run-node: install ## Roda dist/bin/node (headless, so log). Uso: SCENARIO=<arquivo.edl>.
	@test -n "$$SCENARIO" || { echo "$(RED)uso: make run-node SCENARIO=<arquivo.edl|.edl.in>"; exit 1; }
	$(DEST_DIR)/bin/node "$$SCENARIO"

run-node-monitor: install ## Idem run-node, com o Monitor do Groot. Uso: PLAYER= SCENARIO=<arquivo>.
	@test -n "$$PLAYER" || { echo "$(RED)uso: make run-node-monitor PLAYER=<nome-do-player> SCENARIO=<arquivo.edl|.edl.in>"; exit 1; }
	@test -n "$$SCENARIO" || { echo "$(RED)uso: make run-node-monitor PLAYER=<nome-do-player> SCENARIO=<arquivo.edl|.edl.in>"; exit 1; }
	MIXR_GROOT_MONITOR="$$PLAYER" $(DEST_DIR)/bin/node "$$SCENARIO"

venv-rl: ## Cria/atualiza o venv do wrapper Gymnasium em src/rl/.venv.
	python3 -m venv src/rl/.venv
	src/rl/.venv/bin/pip install -q --upgrade pip
	src/rl/.venv/bin/pip install -q -r src/rl/requirements.txt
	@echo "$(GREEN)venv-rl: OK$(NC) -> src/rl/.venv (ative com 'source src/rl/.venv/bin/activate', ou use direto: src/rl/.venv/bin/python3)"

test-rl: install venv-rl ## Roda os testes Python do wrapper Gymnasium, no venv de 'venv-rl'.
	PYTHONPATH=$(DEST_DIR)/python src/rl/.venv/bin/python3 src/rl/tests/test_smoke.py
	PYTHONPATH=$(DEST_DIR)/python src/rl/.venv/bin/python3 src/rl/tests/test_contract.py
	PYTHONPATH=$(DEST_DIR)/python src/rl/.venv/bin/python3 src/rl/tests/test_bad_player.py

venv-rl-training: ## Cria/atualiza o venv de treino de src/poc/rl-training (separado do venv-rl).
	$(MAKE) -C src/poc/rl-training venv

# ============================================
# Test Targets
# ============================================

test-models: ## Roda a suite de CADA projeto de modelo descoberto.
	@# MESMA lista de 'models:' -- MODELOS_PRODUCAO (descoberto por find na
	@# hora, ver o bloco de comentario acima daquele alvo) mais template/,
	@# que nunca e producao mas tem suite propria e precisa continuar
	@# passando: e' o UNICO ponto de partida copiavel ('make new-model'), e
	@# quebra-lo em silencio quebra todo modelo gerado dali em diante.
	@# Mesma forma do laco de 'clean:', pelo mesmo motivo (template/ e'
	@# excluido de MODELOS_PRODUCAO por path, entao entra explicito).
	@#
	@# ACHADO POR AUDITORIA, CORRIGIDO (nao redescobrir): aqui havia um
	@# '$$(MAKE) -C models/players/A-4 test' com o nome CRAVADO. Enquanto
	@# houvesse um modelo de producao so, isso dava exatamente o mesmo
	@# resultado que o laco abaixo -- e por isso passava despercebido. O
	@# modo de falha aparece no SEGUNDO modelo: 'make models' ja o compila
	@# (descoberta por find) e 'make test-models' ignoraria a suite dele em
	@# SILENCIO, que e' a mesma classe de verde-vazio que a guarda de
	@# contagem minima de cada modelo existe para evitar um nivel abaixo.
	@# A suite do template tambem nunca rodava por aqui (5 testes, medidos
	@# passando na mudanca).
	@#
	@# O 'test' de cada Makefile-filho ja confere a contagem minima da
	@# PROPRIA suite (>=3 no A-4, >=4 no template -- numeros por-modelo, de
	@# proposito, por isso 'test' fica fora de models/common.mk) e ja builda
	@# se precisar ('test: build', la) -- nao ha o que duplicar aqui.
	@# Falha rapido ('|| exit 1'), igual ao laco de 'models:'.
	@for d in $(MODELOS_PRODUCAO) models/players/template; do \
	   $(MAKE) -C $$d test || exit 1; \
	 done
	@echo "$(GREEN)test-models: OK$(NC) -> $(words $(MODELOS_PRODUCAO)) de producao ($(notdir $(MODELOS_PRODUCAO))) + template/"

test: install ## Roda SO a suite do HOST (requer -Dtests=true; modelo: 'test-models').
	@# Duas suites do host (memory-controle-negativo, plugin-hotswap) linkam
	@# DIRETO em models/players/A-4/build/ -- libmodel_leak.so/
	@# libmodel_variant_{a,b}.so, nunca instalados, so existem quando o
	@# modelo foi configurado com '-Dvariants=true'. 'install' (acima) NAO
	@# garante isso -- 'sync-plugins' so copia plugins/*.so pra dist/, sem
	@# tocar o build do modelo (decoplado de proposito, ver 'models:'/
	@# 'sync-plugins:'). E se 'make test-models' rodou ANTES desta chamada,
	@# o build do modelo pode ter sido reconfigurado de volta pro default
	@# 'variants=false' (o Makefile de models/players/A-4 nao recebe
	@# VARIANTS=true por padrao) -- derrubando os dois testes acima em
	@# silencio. Reasserta 'variants=true' aqui, incondicional e barato (o
	@# guard STALE de models/common.mk so reconfigura/recompila de verdade
	@# quando algo de fato mudou desde a ultima chamada).
	@$(MAKE) --no-print-directory -C models/players/A-4 build VARIANTS=true ASAN=$(ASAN)
	@N=$$(meson introspect --tests $(BUILD_DIR) | python3 -c 'import json,sys; print(len(json.load(sys.stdin)))'); \
	 [ "$$N" -ge 10 ] || { echo "$(RED)suite do host vazia ou incompleta ($$N) -- configure com -Dtests=true$(NC)"; exit 1; }
	meson test -C $(BUILD_DIR) --print-errorlogs

test-asan: ## Roda a flight sob AddressSanitizer/LeakSanitizer (build separado, lento).
	@# Os DOIS lados: com o modelo num projeto a parte, instrumentar so o host
	@# deixaria o .so sem redzone de pilha e sem simbolos no relatorio do LSan.
	@# (Host com ASan + plugin sem funciona -- os interceptadores vivem na
	@# libasan, no escopo global -- mas o relatorio fica cego para o plugin.)
	@echo "  reconfigurando os DOIS projetos com ASan ..."
	@# 'make models ASAN=true' e nao 'meson configure -Dasan=true': o
	@# 'meson configure' dispara um regenerate que REAVALIA as dependencias, e
	@# ali o dependency('poc-mixr-sdk') ja falhou. A linha completa de
	@# 'meson setup --reconfigure' passa todas as opcoes de novo e e estavel.
	@# 'sync-plugins' NAO depende mais de 'models' (decoplado de proposito,
	@# ver 'models:'/'sync-plugins:' acima) -- as duas chamadas sao
	@# necessarias: 'models ASAN=true' builda o modelo instrumentado e o
	@# deposita em plugins/, 'sync-plugins' copia plugins/ pra dist/. Chamar
	@# só 'sync-plugins' (como este alvo fazia antes) copiava o .so JA
	@# existente em plugins/ sem recompilar -- 'asan: OK' testando um plugin
	@# sem nenhuma instrumentacao, silenciosamente.
	@$(MAKE) --no-print-directory models ASAN=true
	@$(MAKE) --no-print-directory sync-plugins ASAN=true
	@meson configure $(BUILD_DIR) -Dasan=true
	@meson compile -C $(BUILD_DIR) -j$(NINJA_JOBS)
	@mkdir -p $(BUILD_DIR)/tests-fixtures $(BUILD_DIR)/tests-recordings
	@python3 ./tests/scenario/make_fixture.py --poc flight --mode intruder \
		--out $(BUILD_DIR)/tests-fixtures/flight-intruder.edl.in
	@echo "  rodando 500 frames sob ASan ..."
	@LSAN_OPTIONS=suppressions=./tests/memory/asan.supp \
		ASAN_OPTIONS=detect_leaks=1 \
		$(BUILD_DIR)/app/src/app \
		-f $(BUILD_DIR)/tests-fixtures/flight-intruder.edl.in \
		-threads 1 -deterministic 500 > /dev/null; \
		rc=$$?; \
		echo "  revertendo build/ para nao-ASan ..."; \
		revert_falhou=0; \
		$(MAKE) --no-print-directory models ASAN=false >/dev/null 2>&1 || revert_falhou=1; \
		$(MAKE) --no-print-directory sync-plugins ASAN=false >/dev/null 2>&1 || revert_falhou=1; \
		meson configure $(BUILD_DIR) -Dasan=false >/dev/null 2>&1 || revert_falhou=1; \
		meson compile -C $(BUILD_DIR) -j$(NINJA_JOBS) >/dev/null 2>&1 || revert_falhou=1; \
		if [ $$revert_falhou -ne 0 ]; then \
			echo "asan: ATENCAO -- a reversao de build/ para nao-ASan FALHOU (algum dos" >&2; \
			echo "  passos 'models ASAN=false'/'sync-plugins ASAN=false'/'meson configure" >&2; \
			echo "  -Dasan=false'/'meson" >&2; \
			echo "  compile' retornou erro). build/ pode ter ficado instrumentado com ASan --" >&2; \
			echo "  rode 'make configure && make build' manualmente antes de confiar no" >&2; \
			echo "  proximo 'make test'/'make run-*'." >&2; \
		fi; \
		if [ $$rc -eq 0 ]; then echo "asan: OK (sem vazamento reportado)"; \
		else echo "asan: FALHOU (rc=$$rc)"; fi; \
		if [ $$rc -ne 0 ] || [ $$revert_falhou -ne 0 ]; then exit 1; fi

test-ci: ## Roda o .gitlab-ci.yml inteiro num container Docker (Docker + Node).
	@# Fora da suite de proposito, mesmo criterio de 'test-asan'/'test-rl':
	@# '.gitlab-ci.yml' NUNCA usa o remote Conan privado (de proposito -- ver o
	@# comentario do topo daquele arquivo), entao o job 'build' sempre builda
	@# mixr/behaviortree.cpp.asa/jsbsim/openrti/groot do FONTE (INSTALL.md
	@# secao 7) -- HORAS de relogio na primeira vez. 'make test' tem de
	@# continuar hermetico e rapido.
	@# gitlab-ci-local isola o contexto sozinho (rsync de arquivos rastreados
	@# + nao ignorados pelo git -- 'build/'/'dist/'/'contexts/src/' ficam de
	@# fora por estarem no .gitignore), entao NAO reaproveita build/dist/cache
	@# Conan desta arvore de trabalho -- e essa a pergunta que este alvo
	@# responde: clone limpo + '.gitlab-ci.yml' bastam sozinhos?
	@npx --yes gitlab-ci-local

clean-ci: ## Remove .gitlab-ci-local/ (estado + cache de 'test-ci').
	@# Sem isto, o cache (pacotes Conan, build/dist/plugins/models -- ver
	@# 'cache:' de .gitlab-ci.yml) sobrevive entre chamadas de 'make test-ci'
	@# de proposito (e o que evita refazer tudo do zero toda vez); este alvo
	@# e para quando se quer forcar um zero absoluto de novo (revalidar "os
	@# docs bastam sozinhos").
	@# NAO cobre container Docker orfao de uma execucao INTERROMPIDA no meio
	@# (gitlab-ci-local nao nomeia/rotula os containers que cria -- so rastreia
	@# os IDs em memoria do proprio processo -- entao nao ha filtro confiavel
	@# pra 'docker rm' aqui sem risco de pegar container de outra coisa); nesse
	@# caso, `docker ps` e `docker rm -f <id>` a mao.
	rm -rf .gitlab-ci-local/
	@echo "$(GREEN)clean-ci: OK$(NC) -> .gitlab-ci-local/ removido"

# ============================================
# Documentation Targets
# ============================================

docs: ## Regenera docs/manual/ (catalogo + index.html).
	python3 tools/generate_manual_catalog.py > docs/manual/catalog.generated.js
	node docs/manual/compile.js

open-docs: ## Abre docs/manual/index.html no navegador.
	@scripts/open_browser.sh docs/manual/index.html

# TEMPORARIO -- alvo de conveniencia, fora do fluxo normal do repositorio.
# O slide deck e' ORFAO por natureza (nenhum alvo o GERA, nenhum README aponta
# pra ele -- ver a secao 'docs/' do CLAUDE.md), entao aqui so existe o ABRIR:
# nao ha passo de geracao equivalente ao 'make docs' do docs/manual/, o
# index.html e' escrito a mao e versionado como esta. Ao contrario da pagina do
# manual, esta NAO e' autocontida -- carrega ./content.js (relativo, versionado
# ao lado) e as fontes do Google Fonts pela rede; sem rede ela abre igual, so
# com as fontes de fallback. Remover este alvo quando a apresentacao sair de uso.
open-presentation: ## [TEMPORARIO] Abre docs/presentation/index.html (slide deck).
	@scripts/open_browser.sh docs/presentation/index.html

open-edl-builder: ## Regenera e abre src/ui/edl-builder.html (editor visual de cenario).
	node src/ui/scripts/build.js
	@scripts/open_browser.sh src/ui/edl-builder.html

open-groot: ## Abre o Groot (editor/monitor de arvores BT.CPP) -- ver INSTALL.md secao 4.
	@GROOT_BIN="$$(scripts/find_groot.sh 2>/dev/null)"; \
	if [ -n "$$GROOT_BIN" ] && [ -x "$$GROOT_BIN" ]; then \
		setsid "$$GROOT_BIN" >/dev/null 2>&1 & \
	else \
		echo "$(YELLOW)open-groot:$(NC) pacote groot/1.0.0 nao encontrado no cache Conan -- rode 'conan create ./deps/groot --build=missing --settings=build_type=Release' primeiro (ver INSTALL.md secao 4)."; \
	fi

# ============================================
# Misc Targets
# ============================================
help: ## Lista os alvos deste Makefile (e' o que 'make' sem alvo roda).
	@grep -E '^[a-zA-Z0-9_-]+:.*?## .*$$' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-22s\033[0m %s\n", $$1, $$2}'