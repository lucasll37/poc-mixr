# Instalação — passo a passo a partir de uma máquina limpa

Este roteiro é o que [`tests/docker/`](tests/docker/) executa contra um `ubuntu:24.04` recém-saído
da instalação (`make check-docs-ubuntu24`) para medir se a seção **Pré-requisitos** do
[`README.md`](README.md) basta sozinha. Se algum passo aqui parecer incompleto, esse alvo é onde
a lacuna aparece primeiro.

## 1. Pacotes de sistema (`apt`)

```bash
sudo apt update
sudo apt install -y \
    build-essential ninja-build meson pkg-config \
    git curl perl patch zlib1g-dev \
    python3 python3-venv python3-dev pipx \
    gzip sudo
```

Por que cada um:

- **`build-essential`** — `gcc`/`g++`/`make`. O projeto compila em C++17; qualquer GCC ≥ 7 serve.
- **`ninja-build`**, **`meson`** — o Ubuntu 24.04 já traz Meson 1.3.2 e Ninja 1.11 nos repositórios
  padrão, ambos acima do mínimo exigido — não precisa vir de `pip`.
- **`pkg-config`** — sem ele o `meson setup` de `make configure` falha, mesmo com o Conan tendo
  gerado os `.pc` corretos: é por `pkg-config` que o `meson.build` raiz resolve MIXR, protobuf e
  gtest.
- **`git`**, **`curl`**, **`perl`**, **`patch`**, **`zlib1g-dev`** — o remote público (ConanCenter)
  só publica binário de `mixr/1.0.5` para **GCC 11**; o Ubuntu 24.04 traz **GCC 13**, então o
  `package_id` não casa e o `--build=missing` do `make configure` recompila boost, OpenSSL,
  protobuf, JSBSim e o próprio MIXR **do fonte** — e essas receitas autotools/CMake precisam desse
  ferramental. Sem eles o primeiro `make configure` quebra horas depois de começar, no meio da
  compilação de uma dependência transitiva. Se a máquina tiver GCC 11 disponível (ex.: Ubuntu
  22.04, ou um `update-alternatives` apontando para ele), o build usa binário pronto e nada disso
  chega a ser exercitado — mas ainda assim vale instalar, é barato.
- **`python3`**, **`python3-venv`**, **`pipx`** — para instalar o Conan (não há pacote `conan` no
  `apt`; ver §2) e para os scripts de teste em `tests/`.
- **`python3-dev`** — cabeçalhos do Python (`Python.h`). `src/rl/bindings/` compila sempre como
  parte do host e linka `pybind11`, que inclui esse header — sem o pacote, `make build` falha com
  *"Python.h: No such file or directory"*.
- **`gzip`** — descomprime os tiles SRTM (`shared/data/terrain/srtm/*.hgt.gz`) na primeira
  execução de qualquer cenário.
- **`sudo`** — `make configure` roda `conan install` com
  `-c tools.system.package_manager:sudo=True` fixo. Numa máquina onde o usuário já tem `sudo`
  configurado isso é transparente; falta só em containers rodando como `root` sem esse binário.

## 2. Conan (via `pipx`, não `pip`)

O Ubuntu 24.04 recusa `pip install` fora de um ambiente virtual (PEP 668 —
`error: externally-managed-environment`), e não há pacote `conan` no `apt`. O caminho testado é
`pipx`, que já isola o próprio venv:

```bash
pipx install conan
pipx ensurepath
exec "$SHELL"          # ou abra um terminal novo, para o PATH do pipx valer
conan --version         # confirma >= 2.0
```

(Alternativa equivalente, sem `pipx`: `python3 -m venv ~/.venvs/conan && ~/.venvs/conan/bin/pip
install conan` e colocar `~/.venvs/conan/bin` no `PATH`.)

## 3. Perfil do Conan (uma vez por máquina)

O Conan 2 **não** cria o perfil `default` sozinho. Sem este passo, o primeiro `make configure`
morre com *"The default build profile doesn't exist"*:

```bash
conan profile detect --force
```

## 4. Remote privado com as dependências que não estão no ConanCenter

`mixr/1.0.5` e `behaviortree.cpp.asa/3.5.6` (ver `conanfile.py`) são forks empacotados **fora** do
ConanCenter — vêm de um remote Conan privado da organização. Sem declará-lo, `make configure` falha
com *"package not found"* (parece erro de versão, não é). Peça o endereço e as credenciais a quem
administra o projeto e rode, uma vez por máquina:

```bash
conan remote add <nome-do-remote> <url-do-remote>
conan remote login <nome-do-remote> <seu-usuario>   # pede a senha/token interativamente
```

Sem TTY (script não interativo, CI) a falta de credencial **não** aparece como erro de autenticação
legível — o Conan tenta perguntar o usuário, encontra `stdin` fechado e morre com *"not resolved:
EOF when reading a line"*, fácil de confundir com problema de rede.

## 5. Checagem final

```bash
gcc --version && meson --version && ninja --version && pkg-config --version && conan --version
```

Com os cinco respondendo, a máquina está pronta para a seção **Build** do [`README.md`](README.md).
O primeiro `make configure` ainda pode demorar — ver a nota sobre GCC 11 vs. GCC 13 em §1 —, mas
as próximas execuções reaproveitam o cache do Conan (`~/.conan2/`) e são rápidas.

## 6. Editor: `clangd` + extensão do VS Code (opcional)

O repositório já vem configurado para **clangd** (`.clangd`, `.vscode/settings.json` e
`.vscode/extensions.json`), não para o IntelliSense nativo do C/C++ da Microsoft
(`C_Cpp.intelliSenseEngine` já vem `"disabled"`, e `ms-vscode.cpptools` está em
`unwantedRecommendations`). `.clangd` aponta `CompilationDatabase: build` — o
`compile_commands.json` que o **Meson** já gera sozinho em `build/` a cada `make configure`/
`make build` (nenhum passo extra); sem esse diretório existir, o clangd não tem o que indexar.

```bash
sudo apt install -y clangd        # o language server em si
code --install-extension llvm-vs-code-extensions.vscode-clangd
```

O VS Code também oferece a instalação da extensão sozinho: ao abrir a pasta, ele lê
`.vscode/extensions.json` e sugere a mesma extensão no painel de extensões recomendadas — o
`code --install-extension` acima é só o atalho por linha de comando.

**Estilo de formatação (`.clang-format`)** — quem formata é o `clang-format`, não o `.clangd`;
o arquivo na raiz espelha o estilo já em uso (recuo de 3 espaços, chave em linha própria para
classe/função mas colada em `if`/`for`/`while`, `namespace` sem recuo, ponteiro colado ao tipo —
o padrão do próprio MIXR). `.clang-tidy`, em contraste, cuida só de lint (hoje restrito a
`readability-*`, o mais permissivo possível).

> **Armadilha confirmada rodando — não redescobrir:** os blocos `BEGIN_SLOTTABLE`/`END_SLOTTABLE`
> e `BEGIN_SLOT_MAP`/`END_SLOT_MAP` (a tabela de slots do EDL, presente em ~16 arquivos do
> projeto) não têm `;` entre as macros — é assim que o framework original já os escreve. Sem
> proteção, `clang-format` interpreta a ausência de `;` como uma "expressão sem fim" e cola tudo
> numa única linha, destruindo a tabela. Por isso cada bloco desses já vem cercado por
> `// clang-format off` / `// clang-format on` no fonte — **preservar esse par ao editar um
> desses blocos**; um `BEGIN_SLOTTABLE`/`BEGIN_SLOT_MAP` novo, sem o par, formata errado na
> primeira vez que alguém rodar "Format Document" em cima dele.
