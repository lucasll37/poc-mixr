.PHONY: clean configure build install package help run-single-thread check-single-thread run-multi-thread check-multi-thread compare-single-multi run-bandit-dis test test-asan

.DEFAULT_GOAL := help

# Custom variables
PWD := $(shell pwd)
BUILD_DIR := ./build
DEST_DIR := $(PWD)/dist

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


build: ## Build all targets in the project.
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

run-bandit-dis: ## Run bandit-dis (bandit1 sozinho: joystick físico ou Autopilot de fallback, emitindo DIS; Tacview 1235). Rode junto com single-thread ou multi-thread.
	$(BUILD_DIR)/src/bandit-dis/src/bandit-dis

# ============================================
# Test Targets
# ============================================

test: ## Roda a suite de testes (meson test). Requer configure com -Dtests=true.
	meson test -C $(BUILD_DIR) --print-errorlogs

test-asan: ## Roda a single-thread sob AddressSanitizer/LeakSanitizer (build separado, lento; supressões em tests/memory/asan.supp).
	@echo "  reconfigurando com ASan ..."
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
		meson configure $(BUILD_DIR) -Dasan=false >/dev/null; \
		meson compile -C $(BUILD_DIR) -j$(NINJA_JOBS) >/dev/null 2>&1; \
		if [ $$rc -eq 0 ]; then echo "asan: OK (sem vazamento reportado)"; \
		else echo "asan: FALHOU (rc=$$rc)"; exit 1; fi

# ============================================
# Misc Targets
# ============================================
help:
	@grep -E '^[a-zA-Z0-9_-]+:.*?## .*$$' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'