.PHONY: clean configure build create create-all list path remove upload help

.DEFAULT_GOAL := help

# Custom variables
PWD := $(shell pwd)
BUILD_DIR := ./build

# Conan package reference (must match name/version in conanfile.py)
PKG_NAME := behaviortree.cpp.asa
PKG_VERSION := 3.5.6
PKG_REF := $(PKG_NAME)/$(PKG_VERSION)

# Build configuration (override on the command line, e.g. `make create BUILD_TYPE=Release`)
BUILD_TYPE := Debug
SHARED := True

# Remote used by `make upload` (override with `make upload REMOTE=<name>`)
REMOTE := conancenter

CONAN_OPTS := \
	--build=missing \
	--settings=build_type=$(BUILD_TYPE) \
	--options=$(PKG_NAME)/*:shared=$(SHARED)

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

configure: ## Resolve dependencies and generate the CMake toolchain into build/.
	conan install ./ $(CONAN_OPTS)

build: configure ## Build the library locally (in build/, without touching the Conan cache).
	conan build ./ $(CONAN_OPTS)

# ============================================
# Conan Cache Targets
# ============================================

create: ## Build and install the package into the local Conan cache (BUILD_TYPE=Debug by default).
	@printf "$(BLUE)Creating $(PKG_REF) [build_type=$(BUILD_TYPE), shared=$(SHARED)]$(NC)\n"
	conan create ./ $(CONAN_OPTS)
	@printf "$(GREEN)$(PKG_REF) is now in the local Conan cache.$(NC)\n"

create-all: ## Create both Debug and Release packages in the local Conan cache.
	$(MAKE) create BUILD_TYPE=Debug
	$(MAKE) create BUILD_TYPE=Release

list: ## List the package revisions currently in the local Conan cache.
	conan list "$(PKG_NAME)/*:*"

path: ## Print the cache folder of the package for the current BUILD_TYPE.
	conan cache path $(PKG_REF)

remove: ## Remove the package from the local Conan cache.
	conan remove "$(PKG_REF)" --confirm

upload: ## Upload the package to a Conan remote (make upload REMOTE=<name>).
	conan upload "$(PKG_REF)" --remote=$(REMOTE) --confirm

# ============================================
# Misc Targets
# ============================================
help:
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'
