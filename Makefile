.PHONY: clean configure build install package help run run-flying-aircraft run-behavior-tree run-bt-autopilot run-jsbsim-6dof run-formation-flight run-radar-detection run-radar-intercept run-event-relay run-chaff-flare run-satellite-constellation run-custom-models check-custom-models run-jsbsim-ubf check-jsbsim-ubf run-native-stack check-native-stack

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
run: ## Run the root mixr-hello example.
	$(BUILD_DIR)/src/poc-mixr

run-flying-aircraft: ## Run poc/01-flying-aircraft.
	$(BUILD_DIR)/poc/01-flying-aircraft/src/flying-aircraft

run-behavior-tree: ## Run poc/02-behavior-tree.
	$(BUILD_DIR)/poc/02-behavior-tree/src/behavior-tree

run-bt-autopilot: ## Run poc/03-bt-autopilot.
	$(BUILD_DIR)/poc/03-bt-autopilot/src/bt-autopilot

run-jsbsim-6dof: ## Run poc/04-jsbsim-6dof (Tacview Real-Time Telemetry on 127.0.0.1:1234).
	$(BUILD_DIR)/poc/04-jsbsim-6dof/src/jsbsim-6dof

run-formation-flight: ## Run poc/05-formation-flight (5-ship, keyboard control, Tacview on 1234).
	$(BUILD_DIR)/poc/05-formation-flight/src/formation-flight

run-radar-detection: ## Run poc/06-radar-detection (native radar detects a target aircraft).
	$(BUILD_DIR)/poc/06-radar-detection/src/radar-detection

run-radar-intercept: ## Run poc/07-radar-intercept (6DOF hunter + native radar + 3 targets + Tacview 1234).
	$(BUILD_DIR)/poc/07-radar-intercept/src/radar-intercept

run-event-relay: ## Run poc/08-event-relay (radar contact relayed via native event()/send(), Tacview 1234).
	$(BUILD_DIR)/poc/08-event-relay/src/event-relay

run-chaff-flare: ## Run poc/09-chaff-flare (6DOF hunter releases chaff/flare, Tacview 1234).
	$(BUILD_DIR)/poc/09-chaff-flare/src/chaff-flare

run-satellite-constellation: ## Run poc/10-satellite-constellation (4 LEO satellites, accelerated time, Tacview 1234).
	$(BUILD_DIR)/poc/10-satellite-constellation/src/satellite-constellation

run-custom-models: ## Run poc/11-custom-models (player/dynamics/systems próprios + BT em tempo crítico, multithread, Tacview 1234).
	$(BUILD_DIR)/poc/11-custom-models/src/custom-models

run-jsbsim-ubf: ## Run poc/12-jsbsim-ubf (6-DOF JSBSim direto + UBF/BehaviorTree + alerta entre aviões, Tacview 1234).
	$(BUILD_DIR)/poc/12-jsbsim-ubf/src/jsbsim-ubf

run-native-stack: ## Run poc/13-native-stack (mesmo cenário da 12, com a pilha nativa do MIXR, Tacview 1234).
	$(BUILD_DIR)/poc/13-native-stack/src/native-stack

check-native-stack: ## Verifica o determinismo da poc/13 (mesmo estado com 1, 2 e 4 threads T/C).
	@BIN=$(BUILD_DIR)/poc/13-native-stack/src/native-stack; \
	OUT=$(BUILD_DIR)/poc13-determinism; mkdir -p $$OUT; \
	for n in 1 2 4; do \
		echo "  rodando 2000 frames com numTcThreads=$$n ..."; \
		$$BIN -threads $$n -deterministic 2000 2>/dev/null | grep '^frame=' > $$OUT/threads-$$n.txt; \
	done; \
	echo "  repetindo a execução de 4 threads ..."; \
	$$BIN -threads 4 -deterministic 2000 2>/dev/null | grep '^frame=' > $$OUT/threads-4b.txt; \
	fail=0; \
	for pair in "threads-4 threads-4b" "threads-1 threads-2" "threads-1 threads-4"; do \
		set -- $$pair; \
		if diff -q $$OUT/$$1.txt $$OUT/$$2.txt > /dev/null; then echo "  OK   $$1 == $$2"; \
		else echo "  FALHA $$1 != $$2"; fail=1; fi; \
	done; \
	if [ $$fail -eq 0 ]; then echo "determinismo: OK (estado idêntico em todas as execuções)"; \
	else echo "determinismo: FALHOU"; exit 1; fi

check-jsbsim-ubf: ## Verifica o determinismo da poc/12 (mesmo estado com 1, 2 e 4 threads T/C).
	@BIN=$(BUILD_DIR)/poc/12-jsbsim-ubf/src/jsbsim-ubf; \
	OUT=$(BUILD_DIR)/poc12-determinism; mkdir -p $$OUT; \
	for n in 1 2 4; do \
		echo "  rodando 2500 frames com numTcThreads=$$n ..."; \
		$$BIN -threads $$n -deterministic 2500 2>/dev/null | grep '^frame=' > $$OUT/threads-$$n.txt; \
	done; \
	echo "  repetindo a execução de 4 threads ..."; \
	$$BIN -threads 4 -deterministic 2500 2>/dev/null | grep '^frame=' > $$OUT/threads-4b.txt; \
	fail=0; \
	for pair in "threads-4 threads-4b" "threads-1 threads-2" "threads-1 threads-4"; do \
		set -- $$pair; \
		if diff -q $$OUT/$$1.txt $$OUT/$$2.txt > /dev/null; then \
			echo "  OK   $$1 == $$2"; \
		else \
			echo "  FALHA $$1 != $$2"; fail=1; \
		fi; \
	done; \
	if [ $$fail -eq 0 ]; then echo "determinismo: OK (estado idêntico em todas as execuções)"; \
	else echo "determinismo: FALHOU"; exit 1; fi

check-custom-models: ## Verifica o determinismo da poc/11 (mesmo estado com 1, 2 e 4 threads T/C).
	@BIN=$(BUILD_DIR)/poc/11-custom-models/src/custom-models; \
	OUT=$(BUILD_DIR)/poc11-determinism; mkdir -p $$OUT; \
	for n in 1 2 4; do \
		echo "  rodando 3000 frames com numTcThreads=$$n ..."; \
		$$BIN -threads $$n -deterministic 3000 2>/dev/null | grep '^frame=' > $$OUT/threads-$$n.txt; \
	done; \
	echo "  repetindo a execução de 4 threads ..."; \
	$$BIN -threads 4 -deterministic 3000 2>/dev/null | grep '^frame=' > $$OUT/threads-4b.txt; \
	fail=0; \
	for pair in "threads-4 threads-4b" "threads-1 threads-2" "threads-1 threads-4"; do \
		set -- $$pair; \
		if diff -q $$OUT/$$1.txt $$OUT/$$2.txt > /dev/null; then \
			echo "  OK   $$1 == $$2"; \
		else \
			echo "  FALHA $$1 != $$2"; fail=1; \
		fi; \
	done; \
	if [ $$fail -eq 0 ]; then echo "determinismo: OK (estado idêntico em todas as execuções)"; \
	else echo "determinismo: FALHOU"; exit 1; fi

# ============================================
# Misc Targets
# ============================================
help:
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'