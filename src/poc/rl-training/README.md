# src/poc/rl-training -- o consumidor que treina RL de fato contra `src/rl`

Sob `src/poc/` por escolha, mas de natureza diferente das outras pastas dali
([single-thread](../single-thread/)/[multi-thread](../multi-thread/)/etc. --
executaveis C++ com `main.cpp`, ver a tabela em `CLAUDE.md`): esta pasta nao
produz nenhum binario, e so Python. Separada de [`src/rl`](../../rl/) pelo
mesmo motivo que separa `models/player/A4` (o modelo) de `src/poc/*` (quem
consome): `src/rl` e so o AMBIENTE (`mixr_gym.MixrFlightEnv`, contrato
reset/step/observacao); aqui e onde entram as dependencias de treino e o
script que de fato roda um algoritmo de RL contra ele.

Nao entra no grafo do Meson. O unico pre-requisito nativo e o mesmo de
sempre: `make configure && make sdk && make build && make install`, pra
`dist/python/mixr_gym` existir.

## Makefile AUTOCONTIDO

Mesmo padrao dos `Makefile` de `models/*` (ver `models/player/fixtures/stub/Makefile`
para o "porque" completo): entre nesta pasta e rode `make` direto, sem
precisar do Makefile raiz depois do pre-requisito unico (`dist/python/mixr_gym`
publicado). O alvo `venv-rl-training` do Makefile raiz so DELEGA pra `venv`
aqui.

```bash
cd src/poc/rl-training
make help     # lista os alvos
make venv     # cria .venv/ e instala requirements.txt (gymnasium+numpy+stable-baselines3+torch)
```

Separado do `venv-rl` de `src/rl/` de proposito: o ambiente nao precisa saber
com que algoritmo alguem vai treinar contra ele, entao as dependencias de
treino ficam so aqui, nunca em `src/rl/requirements.txt`.

## Treinar

`train.py` e um caso de uso completo, nao um trecho solto: PPO de verdade
(Stable-Baselines3) contra a `Station` de verdade, com checkpoints
periodicos e salvamento no `Ctrl+C` (o episodio inteiro depende do JSBSim
integrando em tempo real de CPU -- perder progresso a uma interrupcao no
meio de uma corrida longa custa caro).

```bash
make train                                    # 200k passos, default
make train ARGS="--timesteps 500000 --seed 1"
```

Ou direto, sem o Makefile (rode com cwd na RAIZ do repositorio -- ver o
cabecalho de `train.py`):

```bash
PYTHONPATH=./dist/python src/poc/rl-training/.venv/bin/python3 \
    src/poc/rl-training/train.py --timesteps 500000
```

Checkpoints saem em `./runs/checkpoints/` (periodicos) e `./runs/ppo_<player>.zip`
(final ou parcial, se interrompido) -- gitignored, nunca versionados.

**Treina com `MlpPolicy`, nao `MultiInputPolicy`** -- [`flatten_obs.py`](flatten_obs.py)
tem um `FlattenedObservation` que embrulha `MixrFlightEnv` e achata o
`Dict`/`Discrete` dela num `Box(28,)` plano, na ordem canonica do C++.
MEDIDO QUEBRANDO ao escrever isto: uma `MultiInputPolicy` treinada direto
sobre o `Dict` nao exporta para o contrato `.onnx` de producao
(`float32[1,28] -> float32[1,3]`) -- o `CombinedExtractor` do SB3 exige um
dict na entrada, e mesmo contornando isso o vetor de features dele nao tem
28 posicoes (os 5 campos booleanos viram one-hot, 10 posicoes). O wrapper
fica no CONSUMIDOR -- o `Dict`/`Discrete` continua sendo o contrato publico
de `src/rl` para quem quiser usa-lo direto. `train.py` e
[`notebooks/train.ipynb`](notebooks/train.ipynb) (ver abaixo) importam o
MESMO `flatten_obs.py` -- nao ha duplicata dessa logica.

## Exportar para producao

`tools/export_onnx.py` converte um checkpoint treinado em `.onnx`, no
contrato que `bt/nodes/OnnxPolicyAction` (`models/player/A4`) espera -- ver o
cabecalho do proprio arquivo para o contrato de entrada/saida e por que a
ordem dos 28 campos nunca e escrita a mao aqui.

```bash
make export ARGS="--sb3 runs/ppo_falcon1.zip -o meu_policy.onnx"
```

Esse `.onnx` e o mesmo formato consumido por [`src/poc/onnx-policy`](../onnx-policy/)
(ver a secao 9 do README daquela poc) -- e por qualquer `treeFile:` que
declare um no `( OnnxPolicy )` apontando pra ele.

## Notebook interativo

[`notebooks/train.ipynb`](notebooks/train.ipynb) e o MESMO treino de
`train.py`, celula a celula -- util pra inspecionar observacao/acao, ajustar
hiperparametro e ver progresso sem reiniciar um processo CLI inteiro a cada
mudanca. `make venv` ja instala `ipykernel` (nada extra a fazer):

1. Abra `notebooks/train.ipynb` no VS Code.
2. "Select Kernel" -> "Python Environments" -> aponte para
   `src/poc/rl-training/.venv` -- o VS Code detecta qualquer interprete com
   `ipykernel` instalado automaticamente, **nao precisa** rodar
   `python -m ipykernel install --user ...` a mao.
3. Rode as celulas **em ordem**, de cima pra baixo, na primeira vez -- o
   notebook tem a MESMA armadilha de ordem de import de `train.py`
   (`mixr_gym` antes de `numpy`/`gymnasium`), so que com um risco a mais que
   um script nao tem: o painel **Variables/Data Viewer do VS Code injeta
   import de numpy/pandas no kernel** por conta propria, pra inspecionar
   variaveis -- se ele for aberto ANTES da celula de `import mixr_gym`, pode
   disparar a mesma armadilha por um caminho invisivel no `.ipynb`. A
   primeira celula de codigo tem uma guarda (`assert`) que transforma essa
   contaminacao num erro claro em vez de um segfault; se ela disparar,
   **reinicie o kernel** (Restart) -- so re-executar a celula nao basta, o
   processo ja esta contaminado.

**Armadilha conhecida do VS Code+WSL2 (`microsoft/vscode-jupyter#9416`),
inofensiva:** às vezes a extensao tenta reinstalar pip/ipykernel mesmo com
`make venv` ja rodado, por confusao de path entre `distutils`/`setuptools`
do venv vs. do sistema. Deixe rodar (idempotente) ou cancele e selecione o
interprete de novo.

`OUT_DIR`/checkpoints do notebook caem no MESMO lugar que `train.py`
(`./runs/`, ancorado nesta pasta, nao em `cwd`) -- ver a nota sobre
`DEFAULT_OUT_DIR` no cabecalho de `train.py` para o "porque" (uma string
relativa tipo `"./runs"` escreveria em `<raiz-do-repo>/runs/`, fora do
`.gitignore` desta pasta, porque tanto `train.py` quanto o notebook mudam
`cwd` pra raiz do repositorio -- MEDIDO acontecendo antes deste fix).

## Limites herdados de `src/rl`

Os mesmos do ambiente (ver `src/rl/README.md`, secao "Limites conhecidos"):
uma `Station` por processo, `import mixr_gym` antes de `numpy`/`gymnasium`,
um agente RL por processo, um frame de latencia na atuacao.
