.PHONY: clean configure sdk models sync-plugins build install package help test-models check-plugin-hotswap run-app run-app-monitor run-node run-node-monitor venv-rl test-rl venv-rl-training test test-asan test-ci clean-ci docs open-docs open-presentation open-edl-builder open-groot new-model

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
NAME ?=
PLAYER ?=
SCENARIO ?=
ARGS ?=
CATEGORY ?=
export NAME PLAYER SCENARIO ARGS CATEGORY

# Colors for output
RED := \033[0;31m
GREEN := \033[0;32m
YELLOW := \033[1;33m
NC := \033[0m # No Color

# ============================================
# C++ Build Targets
# ============================================

clean: ## Clean all generated build files in the project (host + TODOS os modelos descobertos + template + o deposito que 'make models' gerou).
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
	@rm -rf $(PLUGINS_DIR)/data

configure: ## Configure the project for building.
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


sdk: ## Publica o SDK de plugin em dist/ (contrato + libxboard/libxlog/libxtrack/libxrlbridge/libxinfer/libevents). Etapa PRÉVIA ao build do modelo.
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

models: sdk ## Compila e deposita TODOS os modelos de producao (descobertos por find sob QUALQUER subpasta de models/, exceto template/ e o contrato models/events/) em plugins/ -- NAO toca dist/ (ver 'sync-plugins'/'install').
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

sync-plugins: ## Sincroniza plugins/ (proprios + terceiros) para dist/ -- so aqui um cenario enxerga o modelo.
	@# plugins/ ja mistura o que os tres modelos locais depositaram
	@# (via 'models', acima) com qualquer .so de terceiro (ver
	@# plugins/README.md) -- dali em diante os dois sao INDISTINGUIVEIS,
	@# e essa e a ideia: o mesmo passo de copia cobre os dois casos. Pasta
	@# vazia (build limpo, nada depositado) e um no-op silencioso, nao erro.
	mkdir -p $(DEST_DIR)/lib/mixr-plugins/
	@if ls $(PLUGINS_DIR)/*.so >/dev/null 2>&1; then \
	   cp -v $(PLUGINS_DIR)/*.so $(DEST_DIR)/lib/mixr-plugins/; \
	   for so in $(PLUGINS_DIR)/*.so; do \
	      base=$$(basename "$$so"); \
	      ldd $(DEST_DIR)/lib/mixr-plugins/$$base | grep -q 'not found' && { echo "$(RED)sync-plugins: $$base com dependencia nao resolvida$(NC)"; exit 1; } || true; \
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
	 fi
	@echo "$(GREEN)sync-plugins: OK$(NC) -> $(DEST_DIR)/lib/mixr-plugins/, $(DEST_DIR)/share/mixr-plugins/"

# ============================================
# Scaffold de modelo novo
# ============================================

new-model: ## Gera um modelo novo em models/<CATEGORY>/NAME/ a partir de template/ (NAME= e CATEGORY=player|system|others obrigatorios). Ver CONTRIBUTING.md.
	@test -n "$$NAME" || { echo "$(RED)uso: make new-model NAME=meu_modelo CATEGORY=player|system|others$(NC)"; exit 1; }
	@test -n "$$CATEGORY" || { echo "$(RED)uso: make new-model NAME=meu_modelo CATEGORY=player|system|others$(NC)"; exit 1; }
	scripts/models.sh --name "$$NAME" --category "$$CATEGORY"

# ============================================
# Build / Install / Package do HOST
# ============================================

build: sdk ## Compila os executaveis do HOST -- NAO precisa dos modelos (dlopen e so em tempo de EXECUCAO, ver 'install'/'test'/'run-*').
	meson compile -C $(BUILD_DIR) -j$(NINJA_JOBS)

install: build sync-plugins ## Instala os binarios do host em dist/bin/ E sincroniza plugins/ -> dist/ (ver 'sync-plugins').
	@# '--only-changed' -- mesmo "porque" do alvo 'sdk' acima: evita mtime
	@# novo em dist/bin/ sem necessidade a cada chamada.
	meson install -C $(BUILD_DIR) --only-changed

package: ## Create the Conan package for this project.
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

check-plugin-hotswap: install ## Prova que trocar um modelo NÃO recompila a aplicação: muda só o plugin, rebuilda só o .so, e o mesmo binário se comporta diferente.
	@bash tests/plugin/check_hotswap_rebuild.sh

run-app: install ## Run app (TUI; abre a pasta ./sandbox -- sem '-scenario' dentro dela, mostra a tela de seleção de subpastas).
	$(DEST_DIR)/bin/app -folder ./sandbox

run-app-monitor: install ## Roda dist/bin/app com o Monitor ao vivo do Groot ligado para UM player (MIXR_GROOT_MONITOR). Uso: make run-app-monitor PLAYER=falcon1 [ARGS='-folder src/poc/dis -scenario flight'] (default ARGS: '-folder ./sandbox', a tela de selecao). Em outro terminal: 'make open-groot' -> aba Monitor -> localhost, portas 1666 (status) / 1667 (topologia).
	@test -n "$$PLAYER" || { echo "$(RED)uso: make run-app-monitor PLAYER=<nome-do-player>";  exit 1; }
	MIXR_GROOT_MONITOR="$$PLAYER" $(DEST_DIR)/bin/app $${ARGS:--folder ./sandbox}

run-node: install ## Run node (runner headless, sem TUI -- so log no console -- para UM cenario). Uso: make run-node SCENARIO=<arquivo.edl|.edl.in>.
	@test -n "$$SCENARIO" || { echo "$(RED)uso: make run-node SCENARIO=<arquivo.edl|.edl.in>"; exit 1; }
	$(DEST_DIR)/bin/node "$$SCENARIO"

run-node-monitor: install ## Roda dist/bin/node com o Monitor ao vivo do Groot ligado para UM player (MIXR_GROOT_MONITOR). Uso: make run-node-monitor PLAYER=falcon1 SCENARIO=<arquivo.edl|.edl.in>. Em outro terminal: 'make open-groot' -> aba Monitor -> localhost, portas 1666 (status) / 1667 (topologia).
	@test -n "$$PLAYER" || { echo "$(RED)uso: make run-node-monitor PLAYER=<nome-do-player> SCENARIO=<arquivo.edl|.edl.in>"; exit 1; }
	@test -n "$$SCENARIO" || { echo "$(RED)uso: make run-node-monitor PLAYER=<nome-do-player> SCENARIO=<arquivo.edl|.edl.in>"; exit 1; }
	MIXR_GROOT_MONITOR="$$PLAYER" $(DEST_DIR)/bin/node "$$SCENARIO"

venv-rl: ## Cria/atualiza o venv Python LOCAL do wrapper Gymnasium, em src/rl/.venv (gymnasium+numpy -- ver src/rl/requirements.txt). Fora da toolchain Conan/Meson de propósito: nenhum outro alvo depende de Python.
	python3 -m venv src/rl/.venv
	src/rl/.venv/bin/pip install -q --upgrade pip
	src/rl/.venv/bin/pip install -q -r src/rl/requirements.txt
	@echo "$(GREEN)venv-rl: OK$(NC) -> src/rl/.venv (ative com 'source src/rl/.venv/bin/activate', ou use direto: src/rl/.venv/bin/python3)"

test-rl: install venv-rl ## Roda os testes Python do wrapper Gymnasium (src/rl/), usando o venv local criado por 'venv-rl'. Cada script e um PROCESSO -- so pode existir uma Station por processo.
	PYTHONPATH=$(DEST_DIR)/python src/rl/.venv/bin/python3 src/rl/tests/test_smoke.py
	PYTHONPATH=$(DEST_DIR)/python src/rl/.venv/bin/python3 src/rl/tests/test_contract.py
	PYTHONPATH=$(DEST_DIR)/python src/rl/.venv/bin/python3 src/rl/tests/test_bad_player.py

venv-rl-training: ## Delega para o Makefile AUTOCONTIDO de src/poc/rl-training (venv de treino -- separado do venv-rl da biblioteca; ver o "porque" la).
	$(MAKE) -C src/poc/rl-training venv

# ============================================
# Test Targets
# ============================================

test-models: ## Roda a suite do MODELO (domain + tree + native), delegando pro Makefile autocontido de models/players/A-4.
	@# 'test' do Makefile de models/players/A-4 ja confere a contagem (>=3) e ja
	@# builda se precisar (test: build, la) -- nao precisa duplicar aqui.
	$(MAKE) -C models/players/A-4 test

test: install ## Roda SO a suite do HOST (scenario/determinism/plugin/memory/guard/tools/...). Requer configure com -Dtests=true. 'install' builda e sincroniza os modelos (dlopen precisa do .so em dist/) mas NAO roda a suite deles -- para isso, 'make test-models'.
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

test-asan: ## Roda a flight sob AddressSanitizer/LeakSanitizer (build separado, lento; supressões em tests/memory/asan.supp).
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

test-ci: ## Roda '.gitlab-ci.yml' INTEIRO (build + test), do ZERO, num container Docker (via 'npx gitlab-ci-local') -- sucessor de 'check-docs-ubuntu24' (removido): em vez de uma copia paralela dos comandos do README, roda o pipeline de VERDADE, do mesmo jeito que o runner do GitLab roda. Opt-in (precisa de Docker + Node) -- FORA de 'make test'.
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

clean-ci: ## Remove '.gitlab-ci-local/' (estado + cache de 'make test-ci').
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

docs: ## Regenera docs/manual/catalog.generated.js (tools/generate_manual_catalog.py) e docs/manual/index.html a partir de docs/manual/doc.jsx (Babel via docs/manual/compile.js). So precisa de rede na 1a vez (cacheia em docs/manual/.cache/).
	python3 tools/generate_manual_catalog.py > docs/manual/catalog.generated.js
	node docs/manual/compile.js

open-docs: ## Abre docs/manual/index.html no navegador (visualizador animado do ciclo de simulacao MIXR na arvore de componentes). Pagina estatica -- nao depende de build/install.
	@scripts/open_browser.sh docs/manual/index.html

# TEMPORARIO -- alvo de conveniencia, fora do fluxo normal do repositorio.
# O slide deck e' ORFAO por natureza (nenhum alvo o GERA, nenhum README aponta
# pra ele -- ver a secao 'docs/' do CLAUDE.md), entao aqui so existe o ABRIR:
# nao ha passo de geracao equivalente ao 'make docs' do docs/manual/, o
# index.html e' escrito a mao e versionado como esta. Ao contrario da pagina do
# manual, esta NAO e' autocontida -- carrega ./content.js (relativo, versionado
# ao lado) e as fontes do Google Fonts pela rede; sem rede ela abre igual, so
# com as fontes de fallback. Remover este alvo quando a apresentacao sair de uso.
open-presentation: ## [TEMPORARIO] Abre docs/presentation/index.html (slide deck da apresentacao do projeto). So ABRE -- o arquivo e' versionado, nao ha passo de geracao.
	@scripts/open_browser.sh docs/presentation/index.html

open-edl-builder: ## Regenera (catalogo+cenario padrao+testes+self-lint+compilacao, tudo automatico via src/ui/scripts/build.js) e abre src/ui/edl-builder.html no navegador. UNICO alvo make deste editor -- as demais rotinas (geracao do catalogo, do cenario padrao, lint, o binario edlcheck) sao scripts chamados direto, ver src/ui/README.md.
	node src/ui/scripts/build.js
	@scripts/open_browser.sh src/ui/edl-builder.html

open-groot: ## Resolve o pacote groot/1.0.0 no cache Conan (deps/groot/conanfile.py) e abre o Groot -- editor/monitor visual de arvores do BT.CPP v3. Precisa de 'conan create ./deps/groot --build=missing --settings=build_type=Release' rodado antes (ver INSTALL.md secao 7).
	@GROOT_BIN="$$(scripts/find_groot.sh 2>/dev/null)"; \
	if [ -n "$$GROOT_BIN" ] && [ -x "$$GROOT_BIN" ]; then \
		setsid "$$GROOT_BIN" >/dev/null 2>&1 & \
	else \
		echo "$(YELLOW)open-groot:$(NC) pacote groot/1.0.0 nao encontrado no cache Conan -- rode 'conan create ./deps/groot --build=missing --settings=build_type=Release' primeiro (ver INSTALL.md secao 7)."; \
	fi

# ============================================
# Misc Targets
# ============================================
help: ## Lista os alvos deste Makefile, com descricao (e' o que 'make' sem alvo roda).
	@grep -E '^[a-zA-Z0-9_-]+:.*?## .*$$' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'