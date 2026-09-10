.PHONY: clean configure sdk models sync-plugins build install package help test-models run-app run-app-monitor run-node run-node-monitor venv-rl test-rl venv-rl-training test test-asan test-ci clean-ci docs open-docs open-presentation open-edl-builder open-groot new-model rm-model

.DEFAULT_GOAL := help

# Custom variables
PWD := $(shell pwd)
BUILD_DIR := ./build
DEST_DIR := $(PWD)/dist

# Deposito COMPARTILHADO: os modelos deste repositorio E qualquer .so de terceiro
# (ver plugins/README.md). 'make models' so escreve ATE aqui; dist/ e populado
# so por 'make install' (alvo 'sync-plugins'), do HOST.
PLUGINS_DIR := $(PWD)/plugins

# Number of parallel jobs for Ninja (all available cores)
NINJA_JOBS := $(shell nproc)

# Build configuration
BUILD_TYPE := Debug
# ASAN=true reconfigura o projeto do modelo com o sanitizador (ver test-asan).
ASAN ?= false

# Variaveis passadas pelo USUARIO na linha de comando (new-model, rm-model,
# run-app-monitor, run-node, run-node-monitor).
#
# Duas regras que NAO sao cosmeticas:
#
# 1. ':=' e nao '?='. '?=' quer dizer "so se ainda nao estiver definida", e uma
#    variavel de AMBIENTE conta como definida -- o VS Code exporta NAME=Code no
#    terminal integrado, entao 'make new-model CATEGORY=others' (sem NAME=) criava
#    models/others/Code. Com ':=', a atribuicao daqui vence o ambiente e a linha
#    de comando vence a atribuicao daqui, que e a semantica desejada.
#
# 2. As receitas leem '$$NOME' (variavel de SHELL, via o 'export' abaixo), NUNCA
#    '$(NOME)'. '$(NOME)' e substituicao de TEXTO do Make, feita ANTES do shell
#    ver a linha: uma aspa dupla no valor fecha a citacao mais cedo e o resto vira
#    comando. Um guard de validacao na MESMA receita nao protege -- a substituicao
#    acontece nessa linha tambem, entao o comando injetado roda antes do guard.
#
# LIMITE CONHECIDO, nao fechavel daqui: o item 2 fecha metacaracteres de SHELL,
# nao a sintaxe de FUNCAO do proprio Make. 'make <qualquer-alvo> NOME=$(shell
# touch /tmp/x)' executa o touch, porque o GNU Make expande o valor de uma
# atribuicao de linha de comando ao fazer o parse do argv, antes de ler qualquer
# regra deste arquivo -- nao ha gancho que rode antes disso. Fecha-lo exigiria
# nao expor variavel de linha de comando nenhuma. Risco aceito: alcanca so quem
# controla o argv de uma chamada de make.
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
	@# Todo modelo descoberto (MODELOS_PRODUCAO, ver 'models:') + template/, que
	@# nunca entra ali mas pode ter sido construido a mao. 'uninstall-host' roda
	@# ANTES do 'clean' de cada um: ele precisa do ./dist LOCAL intacto para saber
	@# quais basenames remover de plugins/. '|| true' porque um clean antes do
	@# primeiro 'make models' nao tem o que limpar.
	@for d in $(MODELOS_PRODUCAO) models/template; do \
	   $(MAKE) -C $$d uninstall-host 2>/dev/null || true; \
	   $(MAKE) -C $$d clean 2>/dev/null || true; \
	 done
	@# Nunca um 'rm -rf' do namespace de dados inteiro: o laco acima ja removeu,
	@# por NOME, o que cada modelo publicou. Aqui so se recolhe o diretorio se ele
	@# ficou VAZIO -- 'rmdir' falha sozinho, sem estrago, se houver dado de terceiro.
	@rmdir $(PLUGINS_DIR)/data 2>/dev/null || true

configure: ## Configura o projeto para build (conan install + meson setup).
	mkdir -p $(BUILD_DIR)/
	conan install ./ \
		--build=missing \
		--settings=build_type=$(BUILD_TYPE) \
		-c tools.system.package_manager:mode=install \
		-c tools.system.package_manager:sudo=True

	@# O meson DESCARTA o PKG_CONFIG_PATH do ambiente quando o native-file do Conan
	@# fixa 'pkg_config_path' -- tem de ir por linha de comando, e o separador de
	@# lista do meson e VIRGULA, nao dois-pontos.
	@#
	@# '--reconfigure' e condicional: exige um build tree do meson ja existente
	@# (meson-private/coredata.dat) e aborta sem ele -- o caso do primeiro configure
	@# depois de um 'make clean', em que build/ so tem a saida do Conan. Com o tree
	@# presente ela e OBRIGATORIA, senao o meson so imprime "Directory already
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
	@# 'meson compile' resolve por NOME de alvo; o ninja cru so por CAMINHO de
	@# saida ('ninja xboard' -> "unknown target").
	meson compile -C $(BUILD_DIR) xboard xlog xtrack xrlbridge xinfer xpyembed events
	@# '--tags sdk,devel' e nao '--tags sdk': install_headers() nao aceita
	@# install_tag no meson 1.2, entao os headers ficam com a tag automatica
	@# 'devel' -- com '--tags sdk' sozinho o SDK sai SEM headers, e o erro so
	@# aparece dois alvos depois como "PluginAbi.hpp: No such file or directory".
	@# '--only-changed': sem isto cada 'make sdk' recopia com mtime NOVO mesmo sem
	@# mudanca, e o ninja dos modelos relinka os plugins na chamada seguinte.
	meson install -C $(BUILD_DIR) --no-rebuild --tags sdk,devel --only-changed
	@test -f $(DEST_DIR)/lib/pkgconfig/poc-mixr-sdk.pc || { echo "$(RED)sdk: .pc ausente$(NC)"; exit 1; }
	@test -f $(DEST_DIR)/include/xplugin/PluginAbi.hpp || { echo "$(RED)sdk: headers ausentes -- use --tags sdk,devel$(NC)"; exit 1; }
	@echo "$(GREEN)sdk: OK$(NC) -> dist/include, dist/lib, dist/lib/pkgconfig"

# ==============================================================================
# 'models' e 'sync-plugins' -- DECOPLADOS de proposito.
#
# 'models' compila e instala todo modelo de producao sob models/ (QUALQUER
# subpasta: players/, others/, systems/...), cada um um projeto Meson autocontido
# -- DESCOBERTO POR FIND, nunca por lista fixa: um modelo novo so precisa existir.
# Delega pro Makefile de cada um em vez de reimplementar o setup aqui. O template
# fica fora de MODELOS_PRODUCAO (nunca e producao), mas o SEGUNDO artefato dele
# (libtemplate_mirror.so) e instalado a parte logo abaixo -- os testes de plugin
# do host o carregam. O resultado pousa em plugins/, o MESMO deposito que um
# terceiro usaria; este alvo NUNCA escreve em dist/.
#
# 'sync-plugins' e a UNICA ponte para dist/ -- copia plugins/*.so para
# dist/lib/mixr-plugins/ e plugins/data/ para dist/share/mixr-plugins/. So roda
# como parte de 'install': nem 'build' nem 'models' tocam dist/ sozinhos.
# ==============================================================================

# MESMA descoberta de tests/guard/check_modelo_estrutura.sh: todo diretorio sob
# models/ com um project() na raiz do proprio meson.build. Esse filtro ja exclui
# sozinho o que nao e modelo -- um meson.build de subdiretorio (tests/, tools/) so
# tem subdir(), e models/events/ e contrato/SDK consumido por subdir() do host.
# A unica exclusao por PATH, alem de build/dist/subprojects, e template/: ele TEM
# project() (compila e testa sozinho) mas nunca e producao.
#
# O grep casa 'project' sem o '(' de proposito: um '(' solto dentro de um
# '$(shell ...)' desbalanceia a contagem de parenteses que o proprio parser do
# GNU Make faz ("unterminated call to function 'shell': missing )").
MODELOS_PRODUCAO := $(shell find models -mindepth 2 -name meson.build \
                       -not -path '*/build/*' -not -path '*/dist/*' -not -path '*/subprojects/*' \
                       -not -path '*/template/*' -not -path '*/tests/*' -not -path '*/tools/*' \
                       -exec grep -q '^project' {} \; -print \
                     | xargs -r -n1 dirname | sort -u)

models: sdk ## Compila os modelos e deposita em plugins/ -- nao toca dist/ (ver 'install').
	@for d in $(MODELOS_PRODUCAO); do \
	   $(MAKE) -C $$d install-host TESTS=true VARIANTS=true ASAN=$(ASAN) || exit 1; \
	 done
	@# template/ nunca e producao, mas o SEGUNDO artefato dele
	@# (libtemplate_mirror.so) precisa estar em plugins/ para os testes de plugin
	@# do host (plugin-modelo-estranho/plugin-deposito-terceiro).
	@$(MAKE) -C models/template install-host TESTS=true ASAN=$(ASAN) || exit 1
	@echo "$(GREEN)models: OK$(NC) -> $(PLUGINS_DIR)/ ($(words $(MODELOS_PRODUCAO)) projeto(s) de producao: $(notdir $(MODELOS_PRODUCAO)); + template/; rode 'make install' para sincronizar com dist/)"

sync-plugins: ## Copia plugins/ -> dist/ -- so aqui um cenario enxerga o modelo.
	@# plugins/ ja mistura o que os modelos locais depositaram com qualquer .so de
	@# terceiro, e dali em diante os dois sao INDISTINGUIVEIS -- o mesmo passo de
	@# copia cobre os dois casos. Pasta vazia e no-op silencioso, nao erro.
	@#
	@# ESPELHO, nao copia acumulativa: alem de copiar, PODA de dist/ o que nao
	@# existe mais em plugins/. Sem a poda, o .so de um modelo removido ficava em
	@# dist/ para sempre e era recopiado a cada 'make install'. A comparacao e
	@# sempre plugins/ contra dist/, nunca contra uma lista de nomes conhecidos.
	@#
	@# A poda fica DENTRO do ramo de deposito nao-vazio: deposito vazio significa
	@# "modelos ainda nao construidos", e podar ali esvaziaria dist/ num
	@# 'configure && build && install' sem 'make models' -- 'install' NAO depende
	@# de 'models', de proposito.
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
	@# Dados (hoje so o flight: jsbsim/ + flight_tree.xml) -- unica excecao ao
	@# deposito flat de plugins/, documentada em plugins/README.md.
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
	@# Aviso (NUNCA erro) sobre .so em plugins/ que nenhum modelo vivo reclama --
	@# a deteccao mais cedo possivel do orfao de um modelo apagado a mao. E aviso
	@# e nao falha porque "nao reivindicado" inclui, legitimamente, um .so de
	@# terceiro.
	@reclamados=""; \
	 for d in $(MODELOS_PRODUCAO) models/template; do \
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

# Dois modos, e a diferenca importa: com NAME a pasta tem de existir (e dela que
# saem os nomes dos artefatos); com SO voce remove um artefato orfao cuja fonte ja
# nao existe mais. O script recusa se algum cenario ainda referenciar o modelo.
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

# Nao ha alvo de run/check por poc ou cenario particular -- use
# './app -folder <pasta> -scenario <nome>' / '-f <arquivo>' direto, e
# 'tests/determinism/check_determinism.sh' para determinismo. 'make install'
# continua sendo o pre-requisito (dlopen do modelo so em tempo de execucao).
#
# 'check-plugin-hotswap' saiu e NAO deve voltar: a propriedade de runtime que ele
# media ja e afirmada por 'plugin-hotswap' (suite 'plugin'), e "rebuildar so o .so
# nao toca o executavel" e verdadeiro por CONSTRUCAO (host e modelo sao projetos
# meson separados). Em troca, ele editava fonte VERSIONADO com 'sed -i'.

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
	@# MESMA lista de 'models:' (MODELOS_PRODUCAO, descoberto por find) mais
	@# template/, que nunca e producao mas e o UNICO ponto de partida copiavel de
	@# 'make new-model' -- quebra-lo em silencio quebra todo modelo gerado dali em
	@# diante. Nunca uma lista de nomes cravada: com um modelo so daria o mesmo
	@# resultado, e o segundo modelo teria a suite ignorada em SILENCIO.
	@#
	@# O 'test' de cada Makefile-filho ja confere a contagem minima da PROPRIA
	@# suite e ja builda se precisar -- nao ha o que duplicar aqui.
	@for d in $(MODELOS_PRODUCAO) models/template; do \
	   $(MAKE) -C $$d test || exit 1; \
	 done
	@echo "$(GREEN)test-models: OK$(NC) -> $(words $(MODELOS_PRODUCAO)) de producao ($(notdir $(MODELOS_PRODUCAO))) + template/"

test: install ## Roda SO a suite do HOST (requer -Dtests=true; modelo: 'test-models').
	@# Duas suites do host (memory-controle-negativo, plugin-hotswap) linkam DIRETO
	@# em models/players/A-4/build/ -- libmodel_leak.so/libmodel_variant_{a,b}.so,
	@# nunca instalados, so existem com '-Dvariants=true'. 'install' nao garante
	@# isso ('sync-plugins' so copia .so, sem tocar o build do modelo), e um
	@# 'make test-models' anterior pode ter reconfigurado de volta pro default
	@# 'variants=false', derrubando os dois em silencio. Reassertar e barato: o
	@# guard STALE de models/common.mk so reconfigura quando algo de fato mudou.
	@$(MAKE) --no-print-directory -C models/players/A-4 build VARIANTS=true ASAN=$(ASAN)
	@N=$$(meson introspect --tests $(BUILD_DIR) | python3 -c 'import json,sys; print(len(json.load(sys.stdin)))'); \
	 [ "$$N" -ge 10 ] || { echo "$(RED)suite do host vazia ou incompleta ($$N) -- configure com -Dtests=true$(NC)"; exit 1; }
	meson test -C $(BUILD_DIR) --print-errorlogs

test-asan: ## Roda a flight sob AddressSanitizer/LeakSanitizer (build separado, lento).
	@# Os DOIS lados: instrumentar so o host deixaria o .so sem redzone de pilha e
	@# sem simbolos no relatorio do LSan.
	@echo "  reconfigurando os DOIS projetos com ASan ..."
	@# 'make models ASAN=true' e nao 'meson configure -Dasan=true': o 'meson
	@# configure' dispara um regenerate que reavalia as dependencias, e ali o
	@# dependency('poc-mixr-sdk') ja falhou.
	@# As DUAS chamadas sao necessarias ('sync-plugins' nao depende de 'models'):
	@# a primeira builda o modelo instrumentado, a segunda copia pra dist/. So a
	@# segunda copiaria o .so JA existente, dando 'asan: OK' sem instrumentacao.
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
	@# Fora da suite de proposito: o '.gitlab-ci.yml' nunca usa o remote Conan
	@# privado, entao o job 'build' compila mixr/behaviortree/jsbsim/openrti/groot
	@# do FONTE -- horas de relogio na primeira vez. 'make test' tem de continuar
	@# hermetico e rapido.
	@# O gitlab-ci-local isola o contexto sozinho (rsync so do que o git rastreia),
	@# entao NAO reaproveita build/dist/cache desta arvore -- e essa a pergunta que
	@# este alvo responde: clone limpo + '.gitlab-ci.yml' bastam sozinhos?
	@npx --yes gitlab-ci-local

clean-ci: ## Remove .gitlab-ci-local/ (estado + cache de 'test-ci').
	@# O cache (pacotes Conan, build/dist/plugins/models) sobrevive entre chamadas
	@# de 'make test-ci' de proposito; este alvo e para forcar um zero absoluto.
	@# NAO cobre container Docker orfao de uma execucao interrompida no meio -- o
	@# gitlab-ci-local nao nomeia os containers que cria, entao nao ha filtro
	@# confiavel para 'docker rm' aqui. Nesse caso, 'docker ps'/'docker rm -f' a mao.
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

# TEMPORARIO -- o slide deck e orfao por natureza (nenhum alvo o GERA, o
# index.html e escrito a mao e versionado), entao aqui so existe o ABRIR. Ao
# contrario da pagina do manual, esta NAO e autocontida: carrega ./content.js e
# as fontes do Google Fonts pela rede. Remover quando a apresentacao sair de uso.
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