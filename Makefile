.PHONY: clean configure build install package run help

.DEFAULT_GOAL := help

# Custom variables
PWD := $(shell pwd)
BUILD_DIR := ./build
DIST_DIR := $(PWD)/../dist

# Determine number of parallel jobs for Ninja (half of available cores)
NINJA_JOBS := $(shell expr $$(nproc) / 1)

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
		--buildtype debug \
		--buildtype $(shell echo $(BUILD_TYPE) | tr '[:upper:]' '[:lower:]') \
		--native-file $(BUILD_DIR)/conan_meson_native.ini \
		--prefix=$(DIST_DIR) \
		--libdir=$(DIST_DIR)/lib \
		-Db_coverage=true \
		-Dpkg_config_path=$(DIST_DIR)/lib/pkgconfig:$(BUILD_DIR) \
		$(BUILD_DIR)/ .

build: ## Build all targets in the project.
	meson compile -C $(BUILD_DIR) -j$(NINJA_JOBS)

install: ## Install all targets in the project.
	meson install -C $(BUILD_DIR)
	mkdir -p $(DIST_DIR)/models
	cp -r ./models/. $(DIST_DIR)/models/

package: ## Package the project using conan.
	conan create ./ \
		--build=missing \
		--settings=compiler.cppstd=17 \
		--settings=build_type=Debug

	conan create ./ \
		--build=missing \
		--settings=compiler.cppstd=17 \
		--settings=build_type=Release


# ============================================
# Execution Targets
# ============================================

run: ## Run the server.
	$(BUILD_DIR)/src/PocMixr


# ============================================
# Misc Targets
# ============================================

help:
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'