.PHONY: clean configure sdk models build install package help test-models run-single-thread check-single-thread run-multi-thread check-multi-thread compare-single-multi run-bandit-dis test test-asan

.DEFAULT_GOAL := help

# Custom variables
PWD := $(shell pwd)
BUILD_DIR := ./build
MODEL_BUILD_DIR := ./build-models
STUB_BUILD_DIR := ./build-stub
DEST_DIR := $(PWD)/dist
MODEL_DIR := ./models/flight-model

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

clean: ## Clean all generated build files in the project.
	rm -rf $(BUILD_DIR)/
	rm -rf $(MODEL_BUILD_DIR)/
	rm -rf $(STUB_BUILD_DIR)/
	rm -rf $(DEST_DIR)/
	rm -rf ./subprojects/packagecache

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


sdk: ## Publica o SDK de plugin em dist/ (contrato + libxboard/libxlog/libxtrack). Etapa PRÉVIA ao build do modelo.
	@# 'meson compile' com os NOMES dos alvos. Medido: o meson compile resolve
	@# por NOME e traduz para o caminho de saida; o ninja cru resolve so por
	@# CAMINHO ('ninja xboard' -> "unknown target"). Usar o meson aqui evita
	@# ter de escrever shared/xboard/libxboard.so a mao.
	meson compile -C $(BUILD_DIR) xboard xlog xtrack
	@# '--tags sdk,devel' e nao '--tags sdk': install_headers() nao aceita
	@# install_tag no meson 1.2, entao os headers ficam com a tag automatica
	@# 'devel'. Com '--tags sdk' sozinho o SDK sai SEM os headers, e o erro so
	@# aparece dois alvos depois, como "PluginAbi.hpp: No such file or directory".
	meson install -C $(BUILD_DIR) --no-rebuild --tags sdk,devel
	@test -f $(DEST_DIR)/lib/pkgconfig/poc-mixr-sdk.pc || { echo "$(RED)sdk: .pc ausente$(NC)"; exit 1; }
	@test -f $(DEST_DIR)/include/xplugin/PluginAbi.hpp || { echo "$(RED)sdk: headers ausentes -- use --tags sdk,devel$(NC)"; exit 1; }
	@echo "$(GREEN)sdk: OK$(NC) -> dist/include, dist/lib, dist/lib/pkgconfig"

# ASAN=true reconfigura o projeto do modelo com o sanitizador (ver test-asan).
ASAN ?= false

models: sdk ## Compila e instala o MODELO (plugin), num projeto meson à parte. Roda ANTES do build das pocs.
	mkdir -p $(MODEL_BUILD_DIR)/
	meson setup --reconfigure \
		--backend ninja \
		--buildtype $(shell echo $(BUILD_TYPE) | tr '[:upper:]' '[:lower:]') \
		--native-file $(BUILD_DIR)/conan_meson_native.ini \
		--prefix=$(DEST_DIR) \
		--libdir=$(DEST_DIR)/lib \
		-Dpkg_config_path=$(PKG_PATH) \
		-Dtests=true \
		-Dvariants=true \
		-Dasan=$(ASAN) \
		$(MODEL_BUILD_DIR)/ $(MODEL_DIR)
	meson compile -C $(MODEL_BUILD_DIR) -j$(NINJA_JOBS)
	meson install -C $(MODEL_BUILD_DIR) --no-rebuild
	@# O modelo ESTRANHO -- projeto proprio, ve so o SDK. E o unico artefato
	@# que pode falhar por "o contrato nao basta". Ver models/stub-model/CONTRATO.md.
	mkdir -p $(STUB_BUILD_DIR)/
	meson setup --reconfigure \
		--backend ninja \
		--buildtype $(shell echo $(BUILD_TYPE) | tr '[:upper:]' '[:lower:]') \
		--native-file $(BUILD_DIR)/conan_meson_native.ini \
		--prefix=$(DEST_DIR) \
		--libdir=$(DEST_DIR)/lib \
		-Dpkg_config_path=$(PKG_PATH) \
		$(STUB_BUILD_DIR)/ ./models/stub-model
	meson compile -C $(STUB_BUILD_DIR) -j$(NINJA_JOBS)
	meson install -C $(STUB_BUILD_DIR) --no-rebuild
	@for so in libflight_model.so libflight_model_tc.so libstub_model.so; do \
	   ldd $(DEST_DIR)/lib/mixr-plugins/$$so | grep -q 'not found' && { echo "$(RED)models: $$so com dependencia nao resolvida$(NC)"; exit 1; } || true; \
	 done
	@echo "$(GREEN)models: OK$(NC) -> $(DEST_DIR)/lib/mixr-plugins/"

build: models ## Build all targets in the project (o modelo é construído ANTES, como plugin).
	meson compile -C $(BUILD_DIR) -j$(NINJA_JOBS)

install: ## Install all targets into dist/.
	meson install -C $(BUILD_DIR)

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
run-single-thread: ## Run single-thread (decisão no SimAgent nativo da Station, em updateData(); Tacview 1234, teclas +/- acelera/freia, espaço pausa).
	$(BUILD_DIR)/src/single-thread/src/single-thread

check-single-thread: ## Verifica o determinismo do single-thread (mesmo estado com 1, 2 e 4 threads T/C, em cenário hermético — sem DIS).
	@./tests/determinism/check_determinism.sh \
		$(BUILD_DIR)/src/single-thread/src/single-thread single-thread 2000 single-thread

run-multi-thread: ## Run multi-thread (o single-thread trocando só o agente: FlightAgentTC próprio dentro do player, decisão na fase 3; Tacview 1234, teclas +/- e espaço).
	$(BUILD_DIR)/src/multi-thread/src/multi-thread

check-multi-thread: ## Verifica o determinismo do multi-thread (mesmo estado com 1, 2 e 4 threads T/C em cenário hermético, com os 4 agentes em paralelo).
	@./tests/determinism/check_determinism.sh \
		$(BUILD_DIR)/src/multi-thread/src/multi-thread multi-thread 2000 multi-thread

compare-single-multi: ## Lista o que difere entre single-thread e multi-thread (deve ser só o agente do UBF + docs/build).
	@diff -rq --exclude=data --exclude=scenario.generated.epp \
		src/single-thread src/multi-thread || true

check-plugin-hotswap: ## Prova que trocar um modelo NÃO recompila a aplicação: muda só o plugin, rebuilda só o .so, e o mesmo binário se comporta diferente.
	@bash tests/plugin/check_hotswap_rebuild.sh

run-bandit-dis: ## Run bandit-dis (bandit1 sozinho: joystick físico ou Autopilot de fallback, emitindo DIS; Tacview 1235). Rode junto com single-thread ou multi-thread.
	$(BUILD_DIR)/src/bandit-dis/src/bandit-dis

# ============================================
# Test Targets
# ============================================

test-models: ## Roda a suite do MODELO (domain + tree), no projeto dele.
	@# 'meson test' devolve rc=0 para suite VAZIA (medido: "No tests defined.").
	@# Sem esta contagem, perder as duas camadas seria um verde silencioso.
	@N=$$(meson introspect --tests $(MODEL_BUILD_DIR) | python3 -c 'import json,sys; print(len(json.load(sys.stdin)))'); \
	 [ "$$N" -ge 2 ] || { echo "$(RED)suite do modelo vazia ($$N) -- rode 'make models'$(NC)"; exit 1; }
	meson test -C $(MODEL_BUILD_DIR) --print-errorlogs

test: test-models ## Roda a suite INTEIRA: a do modelo e a do host. Requer configure com -Dtests=true.
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
	@$(MAKE) --no-print-directory models ASAN=true
	@meson configure $(BUILD_DIR) -Dasan=true
	@meson compile -C $(BUILD_DIR) -j$(NINJA_JOBS)
	@mkdir -p $(BUILD_DIR)/tests-fixtures $(BUILD_DIR)/tests-recordings
	@python3 ./tests/scenario/make_fixture.py --poc single-thread --mode intruder \
		--out $(BUILD_DIR)/tests-fixtures/single-thread-intruder.epp.in
	@echo "  rodando 500 frames sob ASan ..."
	@LSAN_OPTIONS=suppressions=./tests/memory/asan.supp \
		ASAN_OPTIONS=detect_leaks=1 \
		$(BUILD_DIR)/src/single-thread/src/single-thread \
		-f $(BUILD_DIR)/tests-fixtures/single-thread-intruder.epp.in \
		-threads 1 -deterministic 500 > /dev/null; \
		rc=$$?; \
		$(MAKE) --no-print-directory models ASAN=false >/dev/null 2>&1; \
		meson configure $(BUILD_DIR) -Dasan=false >/dev/null; \
		meson compile -C $(BUILD_DIR) -j$(NINJA_JOBS) >/dev/null 2>&1; \
		if [ $$rc -eq 0 ]; then echo "asan: OK (sem vazamento reportado)"; \
		else echo "asan: FALHOU (rc=$$rc)"; exit 1; fi

# ============================================
# Misc Targets
# ============================================
help:
	@grep -E '^[a-zA-Z0-9_-]+:.*?## .*$$' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'