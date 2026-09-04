.PHONY: clean configure sdk models sync-plugins build install package help test-models run-single-thread check-single-thread run-multi-thread check-multi-thread check-patrol-seed-single-thread check-patrol-seed-multi-thread compare-single-multi run-python-flight check-python-flight run-onnx-policy check-onnx-policy run-bandit run-app venv-rl test-rl venv-rl-training test test-asan check-docs-ubuntu24

.DEFAULT_GOAL := help

# Custom variables
PWD := $(shell pwd)
BUILD_DIR := ./build
DEST_DIR := $(PWD)/dist

# Deposito COMPARTILHADO dos modelos (flight/missile/stub, construidos por
# este repositorio, MAIS qualquer .so de terceiro -- ver
# plugins/README.md). 'make models' so escreve ATE aqui; dist/ e
# populado so por 'make install' (alvo 'sync-plugins'), do HOST.
PLUGINS_DIR := $(PWD)/plugins

# O meson DESCARTA o PKG_CONFIG_PATH do ambiente quando o native-file do Conan
# fixa 'pkg_config_path' (medido). Tem de ir por linha de comando -- e o
# separador de lista do meson e VIRGULA, nao dois-pontos.
PKG_PATH := $(DEST_DIR)/lib/pkgconfig,$(PWD)/build

# Number of parallel jobs for Ninja (all available cores)
NINJA_JOBS := $(shell nproc)

# Build configuration
BUILD_TYPE := Debug

# Colors for output
RED := \033[0;31m
GREEN := \033[0;32m
YELLOW := \033[1;33m
BLUE := \033[0;34m
NC := \033[0m # No Color

# ============================================
# C++ Build Targets
# ============================================

clean: ## Clean all generated build files in the project (host + os tres modelos + o deposito que 'make models' gerou).
	rm -rf $(BUILD_DIR)/
	rm -rf $(DEST_DIR)/
	rm -rf ./subprojects/packagecache
	@# Cada modelo limpa o PROPRIO build/dist (autocontido) -- '|| true' porque
	@# um clean antes do primeiro 'make models' nao tem nada para limpar ali.
	@$(MAKE) -C models/player/flight clean 2>/dev/null || true
	@$(MAKE) -C models/player/missile clean 2>/dev/null || true
	@$(MAKE) -C models/player/fixtures/stub clean 2>/dev/null || true
	@# So os nomes que ESTE repositorio gera -- um .so de TERCEIRO com outro
	@# nome (ver plugins/README.md) nao e apagado por um 'make clean'.
	@rm -f $(PLUGINS_DIR)/libflight.so $(PLUGINS_DIR)/libflight_tc.so \
	       $(PLUGINS_DIR)/libstub.so $(PLUGINS_DIR)/libmissile.so
	@rm -rf $(PLUGINS_DIR)/data

configure: ## Configure the project for building.
	mkdir -p $(BUILD_DIR)/
	conan install ./ \
		--build=missing \
		--settings=build_type=$(BUILD_TYPE) \
		-c tools.system.package_manager:mode=install \
		-c tools.system.package_manager:sudo=True

	meson setup --reconfigure \
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
	@# ter de escrever shared/xboard/libxboard.so a mao.
	meson compile -C $(BUILD_DIR) xboard xlog xtrack xrlbridge xinfer xpyembed events
	@# '--tags sdk,devel' e nao '--tags sdk': install_headers() nao aceita
	@# install_tag no meson 1.2, entao os headers ficam com a tag automatica
	@# 'devel'. Com '--tags sdk' sozinho o SDK sai SEM os headers, e o erro so
	@# aparece dois alvos depois, como "PluginAbi.hpp: No such file or directory".
	@# '--only-changed': sem isto, cada 'make sdk' RECOPIA libxboard/libxlog/
	@# libxtrack pra dist/lib/ com mtime NOVO mesmo sem mudanca de conteudo --
	@# e um destino mais novo que um input ja linkado faz o ninja dos modelos
	@# (flight/missile/stub, que dependem do SDK) achar que precisa RELINKAR
	@# na proxima chamada. Confirmado com 'ninja -C models/player/flight/build -d
	@# explain libflight.so'. Sem isto, TODO 'make install'/'run-*'/'test'
	@# relinkava os quatro plugins de producao, mesmo com tudo ja compilado.
	meson install -C $(BUILD_DIR) --no-rebuild --tags sdk,devel --only-changed
	@test -f $(DEST_DIR)/lib/pkgconfig/poc-mixr-sdk.pc || { echo "$(RED)sdk: .pc ausente$(NC)"; exit 1; }
	@test -f $(DEST_DIR)/include/xplugin/PluginAbi.hpp || { echo "$(RED)sdk: headers ausentes -- use --tags sdk,devel$(NC)"; exit 1; }
	@echo "$(GREEN)sdk: OK$(NC) -> dist/include, dist/lib, dist/lib/pkgconfig"

# ASAN=true reconfigura o projeto do modelo com o sanitizador (ver test-asan).
ASAN ?= false

# ==============================================================================
# 'models' e 'sync-plugins' -- DECOPLADOS de proposito.
#
# 'models' compila e instala flight/missile/stub, cada um projeto Meson
# AUTOCONTIDO (ver models/README.md §1.1) -- delegando pro Makefile de CADA
# um (`$(MAKE) -C models/<nome> install-host`), nao reimplementando o setup
# aqui. O resultado -- so os .so, flat, mais os dados do flight -- pousa em
# plugins/ (e plugins/data/flight/), o MESMO deposito que um
# terceiro usaria (ver plugins/README.md). Este alvo NUNCA escreve em
# dist/ -- e por isso "desacoplado do restante": compilar/instalar um
# modelo nao presume nada sobre onde o HOST guarda os artefatos dele.
#
# 'sync-plugins' e a UNICA ponte para dist/ -- copia plugins/*.so
# (de QUALQUER origem: flight/missile/stub OU terceiro, ja indistinguiveis
# neste ponto) para dist/lib/mixr-plugins/, e plugins/data/ para
# dist/share/mixr-plugins/. So roda como parte de 'install' -- e por isso
# "so no install do restante do projeto": nem 'build' nem 'models' tocam
# dist/lib/mixr-plugins/ sozinhos.
# ==============================================================================

models: sdk ## Compila e deposita flight/missile/stub em plugins/ -- NAO toca dist/ (ver 'sync-plugins'/'install').
	$(MAKE) -C models/player/flight install-host TESTS=true VARIANTS=true ASAN=$(ASAN)
	@# O modelo ESTRANHO -- projeto proprio, ve so o SDK. E o unico artefato
	@# que pode falhar por "o contrato nao basta". Ver models/player/fixtures/stub/CONTRATO.md.
	@# Fica em fixtures/ porque nao e um modelo de producao -- e um fixture de teste.
	$(MAKE) -C models/player/fixtures/stub install-host TESTS=true
	@# O SEGUNDO plugin de exemplo -- so o GuidedMissile, carregado ao lado do
	@# flight no cenario de demo (scenario_missile_demo.epp.in). Mesmo molde
	@# do stub: projeto Meson proprio, so mixr_dep + sdk_dep.
	$(MAKE) -C models/player/missile install-host TESTS=true
	@echo "$(GREEN)models: OK$(NC) -> $(PLUGINS_DIR)/ (rode 'make install' para sincronizar com dist/)"

sync-plugins: models ## Sincroniza plugins/ (proprios + terceiros) para dist/ -- so aqui um cenario enxerga o modelo.
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
	 fi
	@# Dados (hoje, so o flight: jsbsim/ + flight_tree.xml) -- unica excecao
	@# ao deposito flat de plugins/, documentada em
	@# plugins/README.md.
	@if [ -d $(PLUGINS_DIR)/data ]; then \
	   mkdir -p $(DEST_DIR)/share/mixr-plugins/; \
	   cp -a $(PLUGINS_DIR)/data/. $(DEST_DIR)/share/mixr-plugins/; \
	 fi
	@echo "$(GREEN)sync-plugins: OK$(NC) -> $(DEST_DIR)/lib/mixr-plugins/, $(DEST_DIR)/share/mixr-plugins/"

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

# --------------------------------------------------------------------------
# single-thread e multi-thread são o MESMO modelo; o nome diz só ONDE
# a decisão do UBF roda — e NÃO quantas threads a simulação usa. As duas
# declaram numTcThreads e espalham os players pelo pool de threads de tempo
# crítico do framework (é por isso que as duas têm check de determinismo com
# 1, 2 e 4 threads):
#   single-thread: ( SimAgent ) nativo, componente da Station, decide em
#                  updateData() — os 4 em sequência, numa thread de
#                  background, a 10 Hz.
#   multi-thread:  ( FlightAgentTC ) próprio, componente do player, decide na
#                  fase 3 do frame — os 4 em paralelo, um por thread do pool
#                  de tempo crítico, a 50 Hz.
# --------------------------------------------------------------------------
# Todos os alvos abaixo RODAM um binario que faz dlopen() num modelo em
# tempo de execucao -- por isso dependem de 'install' (que roda 'models' +
# 'sync-plugins' + o build do host). 'build'/'models' sozinhos NAO deixam
# nada rodavel: dist/lib/mixr-plugins/ so e populado no install (ver o
# comentario grande acima de 'models'/'sync-plugins').

run-single-thread: install ## Run single-thread (decisão no SimAgent nativo da Station, em updateData(); Tacview 1234, teclas +/- acelera/freia, espaço pausa).
	$(BUILD_DIR)/app/src/app -scenario single-thread

check-single-thread: install ## Verifica o determinismo do single-thread (mesmo estado com 1, 2 e 4 threads T/C, em cenário hermético — sem DIS).
	@./tests/determinism/check_determinism.sh \
		$(BUILD_DIR)/app/src/app single-thread 2000 single-thread

run-multi-thread: install ## Run multi-thread (o single-thread trocando só o agente: FlightAgentTC próprio dentro do player, decisão na fase 3; Tacview 1234, teclas +/- e espaço).
	$(BUILD_DIR)/app/src/app -scenario multi-thread

check-multi-thread: install ## Verifica o determinismo do multi-thread (mesmo estado com 1, 2 e 4 threads T/C em cenário hermético, com os 4 agentes em paralelo).
	@./tests/determinism/check_determinism.sh \
		$(BUILD_DIR)/app/src/app multi-thread 2000 multi-thread

run-python-flight: install ## Run python-flight (a mesma pilha do multi-thread com as leis de voo em Python: configs/policy/*.py, editáveis sem recompilar; Tacview 1237).
	$(BUILD_DIR)/app/src/app -scenario python-flight

check-python-flight: install ## Verifica o determinismo do python-flight (mesmo estado com 1, 2 e 4 threads T/C — os quatro scripts decidem em paralelo, serializados pelo GIL).
	@./tests/determinism/check_determinism.sh \
		$(BUILD_DIR)/app/src/app python-flight 2000 python-flight

run-onnx-policy: install ## Run onnx-policy (a mesma pilha do multi-thread com a decisão numa REDE NEURAL: configs/policy_barrier.onnx, inferido na fase 3 do frame; Tacview 1238).
	$(BUILD_DIR)/app/src/app -scenario onnx-policy

check-onnx-policy: install ## Verifica o determinismo do onnx-policy (mesmo estado com 1, 2 e 4 threads T/C — as quatro aeronaves inferem em paralelo, numa sessão só do ONNX Runtime).
	@./tests/determinism/check_determinism.sh \
		$(BUILD_DIR)/app/src/app onnx-policy 2000 onnx-policy

check-patrol-seed-single-thread: install ## Prova o RNG de patrulha no single-thread: mesma patrolMasterSeed reproduz entre 1/2/4 threads T/C; sementes diferentes divergem, em qualquer thread.
	@./tests/determinism/check_patrol_seed.sh \
		$(BUILD_DIR)/app/src/app single-thread single-thread 600

check-patrol-seed-multi-thread: install ## Prova o RNG de patrulha no multi-thread: mesma patrolMasterSeed reproduz entre 1/2/4 threads T/C; sementes diferentes divergem, em qualquer thread.
	@./tests/determinism/check_patrol_seed.sh \
		$(BUILD_DIR)/app/src/app multi-thread multi-thread 600

compare-single-multi: ## Lista o que difere entre single-thread e multi-thread (hoje só o cenário: o agente do UBF e a porta DIS).
	@diff -rq --exclude=data \
		src/poc/dis/single-thread src/poc/dis/multi-thread || true

check-plugin-hotswap: install ## Prova que trocar um modelo NÃO recompila a aplicação: muda só o plugin, rebuilda só o .so, e o mesmo binário se comporta diferente.
	@bash tests/plugin/check_hotswap_rebuild.sh

run-built-in_mixr_1: install ## Run built-in_mixr_1 (o PLAYER MAXIMO: falcon1 com 53 classes nativas do mixr::models num unico Aircraft; Tacview 1239).
	$(BUILD_DIR)/app/src/app -scenario built-in_mixr_1

check-built-in_mixr_1: install ## Verifica o determinismo do built-in_mixr_1 (mesmo estado com 1, 2 e 4 threads T/C). O cenario proprio ja e hermetico -- sem 'networks:', nao precisa de fixture.
	@./tests/determinism/check_determinism.sh \
		$(BUILD_DIR)/app/src/app built-in_mixr_1 2000 '' built-in_mixr_1

run-bandit: install ## Run bandit (bandit1 sozinho: joystick físico ou Autopilot de fallback, emitindo DIS; Tacview 1235). Rode junto com single-thread ou multi-thread.
	$(BUILD_DIR)/app/src/app -scenario bandit

run-app: ## Run app (TUI; sem '-scenario' mostra a tela de seleção).
	$(BUILD_DIR)/app/src/app

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

test-models: ## Roda a suite do MODELO (domain + tree + native), delegando pro Makefile autocontido de models/player/flight.
	@# 'test' do Makefile de models/player/flight ja confere a contagem (>=3) e ja
	@# builda se precisar (test: build, la) -- nao precisa duplicar aqui.
	$(MAKE) -C models/player/flight test

test: test-models install ## Roda a suite INTEIRA: a do modelo e a do host. Requer configure com -Dtests=true. 'install' garante dist/ populado p/ os testes que rodam binario.
	@N=$$(meson introspect --tests $(BUILD_DIR) | python3 -c 'import json,sys; print(len(json.load(sys.stdin)))'); \
	 [ "$$N" -ge 10 ] || { echo "$(RED)suite do host vazia ou incompleta ($$N) -- configure com -Dtests=true$(NC)"; exit 1; }
	meson test -C $(BUILD_DIR) --print-errorlogs

test-asan: ## Roda a single-thread sob AddressSanitizer/LeakSanitizer (build separado, lento; supressões em tests/memory/asan.supp).
	@# Os DOIS lados: com o modelo num projeto a parte, instrumentar so o host
	@# deixaria o .so sem redzone de pilha e sem simbolos no relatorio do LSan.
	@# (Host com ASan + plugin sem funciona -- os interceptadores vivem na
	@# libasan, no escopo global -- mas o relatorio fica cego para o plugin.)
	@echo "  reconfigurando os DOIS projetos com ASan ..."
	@# 'make models ASAN=true' e nao 'meson configure -Dasan=true': o
	@# 'meson configure' dispara um regenerate que REAVALIA as dependencias, e
	@# ali o dependency('poc-mixr-sdk') ja falhou. A linha completa de
	@# 'meson setup --reconfigure' passa todas as opcoes de novo e e estavel.
	@# 'sync-plugins' ja depende de 'models' (PHONY -- sempre reavalia), entao
	@# uma chamada so basta; ASAN=true propaga por linha de comando ate o
	@# 'install-host ASAN=$(ASAN)' dentro de 'models'.
	@$(MAKE) --no-print-directory sync-plugins ASAN=true
	@meson configure $(BUILD_DIR) -Dasan=true
	@meson compile -C $(BUILD_DIR) -j$(NINJA_JOBS)
	@mkdir -p $(BUILD_DIR)/tests-fixtures $(BUILD_DIR)/tests-recordings
	@python3 ./tests/scenario/make_fixture.py --poc single-thread --mode intruder \
		--out $(BUILD_DIR)/tests-fixtures/single-thread-intruder.epp.in
	@echo "  rodando 500 frames sob ASan ..."
	@LSAN_OPTIONS=suppressions=./tests/memory/asan.supp \
		ASAN_OPTIONS=detect_leaks=1 \
		$(BUILD_DIR)/app/src/app \
		-f $(BUILD_DIR)/tests-fixtures/single-thread-intruder.epp.in \
		-threads 1 -deterministic 500 > /dev/null; \
		rc=$$?; \
		$(MAKE) --no-print-directory sync-plugins ASAN=false >/dev/null 2>&1; \
		meson configure $(BUILD_DIR) -Dasan=false >/dev/null; \
		meson compile -C $(BUILD_DIR) -j$(NINJA_JOBS) >/dev/null 2>&1; \
		if [ $$rc -eq 0 ]; then echo "asan: OK (sem vazamento reportado)"; \
		else echo "asan: FALHOU (rc=$$rc)"; exit 1; fi

check-docs-ubuntu24: ## Levanta um Ubuntu 24.04 LIMPO no Docker e roda nele os comandos do README, para medir se a documentacao basta. Opt-in (precisa de Docker) -- FORA de 'make test'.
	@# Fora da suite de proposito, mesmo criterio de 'test-asan'/'test-rl': depende
	@# de Docker e de rede, e o modo completo leva HORAS (ver o achado
	@# 'sem-binarios-para-gcc13' em tests/docker/gaps_conhecidos.json). 'make test'
	@# tem de continuar hermetico e rapido.
	@# MODO=completo e PERFIL=completo passam direto para o runner. O remote
	@# privado e as credenciais vem do AMBIENTE (CONAN_REMOTE_NOME/URL/USUARIO/
	@# SENHA) -- nenhum endereco de registry esta escrito neste repositorio, e
	@# sem eles o portao correspondente e pulado. Ver tests/docker/README.md.
	@python3 ./tests/docker/run_docs_build_test.py \
		--modo $(or $(MODO),rapido) \
		--perfil $(or $(PERFIL),readme)

# ============================================
# Misc Targets
# ============================================
help:
	@grep -E '^[a-zA-Z0-9_-]+:.*?## .*$$' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'