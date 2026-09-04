.PHONY: venv train export clean check-root help
.DEFAULT_GOAL := help

# ==============================================================================
# Makefile AUTOCONTIDO do consumidor de treino -- ver README.md.
#
# Ao contrario de models/*/Makefile (que compilam um .so via Meson), este
# projeto e so Python -- nao ha configure/build/install de C++ aqui. O
# unico pre-requisito e o HOST publicado uma vez, na raiz do repositorio:
#
#     cd ../../.. && make configure && make sdk && make build && make install
#
# Depois disso este Makefile nunca mais precisa do Makefile raiz -- so
# consome dist/python/mixr_gym (o ambiente, ja pronto) e mantem o proprio
# venv de treino, separado do venv-rl da biblioteca (ver src/rl/README.md
# para o "porque" da separacao: o ambiente nao precisa saber com que
# algoritmo alguem vai treinar contra ele). O alvo 'venv-rl-training' do
# Makefile raiz so DELEGA pra 'venv' aqui -- mesmo padrao de 'make models'
# delegando pros Makefiles de models/*.
# ==============================================================================

ROOT        := $(abspath ../../..)
PWD         := $(shell pwd)
DIST_PYTHON := $(ROOT)/dist/python
VENV        := $(PWD)/.venv
PY          := $(VENV)/bin/python3
RUNS_DIR    := $(PWD)/runs

RED   := \033[0;31m
GREEN := \033[0;32m
NC    := \033[0m

check-root: ## Confere que o HOST ja publicou dist/python/mixr_gym (pre-requisito, uma vez).
	@test -d $(DIST_PYTHON)/mixr_gym || { \
		echo "$(RED)faltando $(DIST_PYTHON)/mixr_gym$(NC)"; \
		echo "  rode 'cd $(ROOT) && make configure && make sdk && make build && make install' primeiro (uma vez)."; \
		exit 1; }

venv: ## Cria/atualiza o venv LOCAL de treino (ver requirements.txt -- gymnasium+numpy+stable-baselines3+torch).
	python3 -m venv $(VENV)
	$(PY) -m pip install -q --upgrade pip
	$(PY) -m pip install -q -r requirements.txt
	@echo "$(GREEN)venv: OK$(NC) -> $(VENV) (ative com 'source .venv/bin/activate', ou use direto: .venv/bin/python3)"

train: check-root venv ## Treina PPO contra MixrFlightEnv e salva o checkpoint em ./runs/ (ver 'train.py --help'; ex.: make train ARGS="--timesteps 500000").
	@mkdir -p $(RUNS_DIR)
	@# cd na RAIZ do repositorio: 'scenario_path' do MixrFlightEnv e
	@# resolvido por caminho relativo a raiz (mesma convencao de todo
	@# binario deste projeto -- ver CLAUDE.md/src/rl/README.md). Interprete
	@# e script vao por caminho ABSOLUTO ($(PY)/$(PWD)), entao o 'cd' abaixo
	@# nao os afeta. '--out-dir' explicito (nao so o default de train.py):
	@# train.py ja ancora o proprio default em '<esta-pasta>/runs' (nao em
	@# cwd), mas passar aqui deixa o Makefile a fonte de verdade do
	@# caminho, sem depender de dois lugares concordarem por coincidencia.
	cd $(ROOT) && PYTHONPATH=$(DIST_PYTHON) $(PY) $(PWD)/train.py --out-dir $(RUNS_DIR) $(ARGS)

export: check-root venv ## Exporta um checkpoint treinado para .onnx (ver 'tools/export_onnx.py --help'; ex.: make export ARGS="--sb3 runs/ppo_falcon1.zip -o politica.onnx").
	@# SEM 'cd $(ROOT)' aqui, ao contrario de 'train' -- export_onnx.py nao
	@# le cenario nenhum (resolve dist/python pelo PROPRIO __file__, nao por
	@# cwd), entao nao ha motivo pra mudar de diretorio. cwd fica nesta
	@# pasta, o que faz os caminhos de ARGS (ex.: 'runs/ppo_falcon1.zip')
	@# resolverem contra ELA -- o mesmo lugar onde 'train' acima escreve.
	PYTHONPATH=$(DIST_PYTHON) $(PY) tools/export_onnx.py $(ARGS)

clean: ## Remove o venv local e os checkpoints de ./runs (NAO mexe no dist/ do host).
	rm -rf $(VENV) $(RUNS_DIR)
	find . -name __pycache__ -type d -exec rm -rf {} +

help:
	@grep -E '^[a-zA-Z0-9_-]+:.*?## .*$$' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-16s\033[0m %s\n", $$1, $$2}'
