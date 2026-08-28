.PHONY: clean configure build install package help run-flying-aircraft run-behavior-tree run-bt-autopilot run-jsbsim-6dof run-formation-flight run-radar-detection run-radar-intercept run-event-relay run-chaff-flare run-satellite-constellation run-custom-models check-custom-models run-jsbsim-ubf check-jsbsim-ubf run-single-thread check-single-thread run-multi-thread check-multi-thread compare-single-multi

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
run-flying-aircraft: ## Run poc/01-flying-aircraft.
	$(BUILD_DIR)/src/01-flying-aircraft/src/flying-aircraft

run-behavior-tree: ## Run poc/02-behavior-tree.
	$(BUILD_DIR)/src/02-behavior-tree/src/behavior-tree

run-bt-autopilot: ## Run poc/03-bt-autopilot.
	$(BUILD_DIR)/src/03-bt-autopilot/src/bt-autopilot

run-jsbsim-6dof: ## Run poc/04-jsbsim-6dof (Tacview Real-Time Telemetry on 127.0.0.1:1234).
	$(BUILD_DIR)/src/04-jsbsim-6dof/src/jsbsim-6dof

run-formation-flight: ## Run poc/05-formation-flight (5-ship, keyboard control, Tacview on 1234).
	$(BUILD_DIR)/src/05-formation-flight/src/formation-flight

run-radar-detection: ## Run poc/06-radar-detection (native radar detects a target aircraft).
	$(BUILD_DIR)/src/06-radar-detection/src/radar-detection

run-radar-intercept: ## Run poc/07-radar-intercept (6DOF hunter + native radar + 3 targets + Tacview 1234).
	$(BUILD_DIR)/src/07-radar-intercept/src/radar-intercept

run-event-relay: ## Run poc/08-event-relay (radar contact relayed via native event()/send(), Tacview 1234).
	$(BUILD_DIR)/src/08-event-relay/src/event-relay

run-chaff-flare: ## Run poc/09-chaff-flare (6DOF hunter releases chaff/flare, Tacview 1234).
	$(BUILD_DIR)/src/09-chaff-flare/src/chaff-flare

run-satellite-constellation: ## Run poc/10-satellite-constellation (4 LEO satellites, accelerated time, Tacview 1234).
	$(BUILD_DIR)/src/10-satellite-constellation/src/satellite-constellation

run-custom-models: ## Run poc/11-custom-models (player/dynamics/systems próprios + BT em tempo crítico, multithread, Tacview 1234).
	$(BUILD_DIR)/src/11-custom-models/src/custom-models

run-jsbsim-ubf: ## Run poc/12-jsbsim-ubf (6-DOF JSBSim direto + UBF/BehaviorTree + alerta entre aviões, Tacview 1234).
	$(BUILD_DIR)/src/12-jsbsim-ubf/src/jsbsim-ubf

# --------------------------------------------------------------------------
# poc/single-thread e poc/multi-thread são o MESMO modelo; o nome diz só ONDE
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
run-single-thread: ## Run poc/single-thread (decisão no SimAgent nativo da Station, em updateData(); Tacview 1234, teclas +/- acelera/freia, espaço pausa).
	$(BUILD_DIR)/src/single-thread/src/single-thread

check-single-thread: ## Verifica o determinismo da poc/single-thread (mesmo estado com 1, 2 e 4 threads T/C — a simulação é multithread aqui também).
	@BIN=$(BUILD_DIR)/src/single-thread/src/single-thread; \
	OUT=$(BUILD_DIR)/single-thread-determinism; mkdir -p $$OUT; \
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

run-multi-thread: ## Run poc/multi-thread (a poc/single-thread trocando só o agente: FlightAgentTC próprio dentro do player, decisão na fase 3; Tacview 1234, teclas +/- e espaço).
	$(BUILD_DIR)/src/multi-thread/src/multi-thread

check-multi-thread: ## Verifica o determinismo da poc/multi-thread (mesmo estado com 1, 2 e 4 threads T/C, mesmo com os 4 agentes decidindo em paralelo).
	@BIN=$(BUILD_DIR)/src/multi-thread/src/multi-thread; \
	OUT=$(BUILD_DIR)/multi-thread-determinism; mkdir -p $$OUT; \
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
	echo "  decisões por frame (tem que bater com o número de frames):"; \
	grep 'player=falcon1 ' $$OUT/threads-4.txt | tail -1 | grep -o 'frame=[0-9]* \|dec=[0-9]*' | tr '\n' ' '; echo; \
	if [ $$fail -eq 0 ]; then echo "determinismo: OK (estado idêntico em todas as execuções)"; \
	else echo "determinismo: FALHOU"; exit 1; fi

compare-single-multi: ## Lista o que difere entre a poc/single-thread e a poc/multi-thread (deve ser só o agente do UBF + docs/build).
	@diff -rq --exclude=data --exclude=scenario.generated.epp \
		src/single-thread src/multi-thread || true

check-jsbsim-ubf: ## Verifica o determinismo da poc/12 (mesmo estado com 1, 2 e 4 threads T/C).
	@BIN=$(BUILD_DIR)/src/12-jsbsim-ubf/src/jsbsim-ubf; \
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
	@BIN=$(BUILD_DIR)/src/11-custom-models/src/custom-models; \
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
	@grep -E '^[a-zA-Z0-9_-]+:.*?## .*$$' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'