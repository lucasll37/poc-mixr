# Instalação — passo a passo a partir de uma máquina limpa

Este roteiro é o que [`.gitlab-ci.yml`](.gitlab-ci.yml) segue, passo a passo, contra um
`ubuntu:24.04` recém-saído da instalação — o job `build` roda exatamente esta sequência antes de
`make configure`. `make test-ci` (ver [`README.md`](README.md), seção "CI (GitLab)") roda esse
pipeline localmente, num container Docker, do zero; se algum passo aqui parecer incompleto, é ali
que a lacuna aparece primeiro.

`make test-ci` é opt-in e exige Docker (+ Node) instalados à parte — não são pré-requisito do
build em si, só desta checagem. Instruções de instalação: [docker.com](https://www.docker.com/).

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

## 6. Editor: VS Code (opcional)

### 6.1. Extensões recomendadas

O repositório declara recomendações em `.vscode/extensions.json` — ao abrir a pasta, o VS Code
mostra um aviso ("This workspace has extension recommendations") e deixa instalar todas de uma vez
pelo painel de Extensões (aba "Recommended"). Sem clicar em nada, instalar uma a uma:

```bash
code --install-extension llvm-vs-code-extensions.vscode-clangd
code --install-extension pkief.material-icon-theme
code --install-extension natqe.reload
code --install-extension anthropic.claude-code
code --install-extension ms-toolsai.jupyter
code --install-extension yzhang.markdown-all-in-one
code --install-extension ms-vscode.cpptools
code --install-extension spencerwmiles.vscode-task-buttons
```

| extensão | para quê |
|---|---|
| `llvm-vs-code-extensions.vscode-clangd` | o language server C++ deste projeto — ver §6.2 |
| `ms-vscode.cpptools` | debugger (`cppdbg`) e tarefas de build da Microsoft — **não** o IntelliSense dela, que `.vscode/settings.json` já desliga (`C_Cpp.intelliSenseEngine: "disabled"`) a favor do clangd |
| `spencerwmiles.vscode-task-buttons` | mostra o botão "$(play) app" na barra de status (task "Run app" de `.vscode/tasks.json`, que roda `./build/app/src/app -folder ./sandbox`) — **essa task assume `gnome-terminal` instalado** (abre o app num terminal externo); em KDE/XFCE/WSL2 sem esse pacote a task falha com "command not found" — rode o binário direto num terminal seu nesse caso |
| `anthropic.claude-code` | a extensão do Claude Code em si |
| `ms-toolsai.jupyter` | notebooks `.ipynb`, se usados em `src/poc/rl-training/` |
| `yzhang.markdown-all-in-one` | edição confortável dos muitos `.md` deste repositório |
| `pkief.material-icon-theme`, `natqe.reload` | cosméticas/conveniência, sem efeito no build |

### 6.2. `clangd` (C++)

O repositório já vem configurado para **clangd** (`.clangd`, `.vscode/settings.json`), não para o
IntelliSense nativo do C/C++ da Microsoft. `.clangd` aponta `CompilationDatabase: build` — o
`compile_commands.json` que o **Meson** já gera sozinho em `build/` a cada `make configure`/
`make build` (nenhum passo extra); sem esse diretório existir, o clangd não tem o que indexar.

```bash
sudo apt install -y clangd        # o language server em si -- a extensao so' fala com ele
```

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

### 6.3. Highlight de `.edl` (extensão local, não vem do Marketplace)

`.vscode/extensions/edl/` é uma extensão de sintaxe para `.edl`/`.edl.in` **vendorizada no próprio
repositório** — só highlight/indentação/colchetes (sem `main`/código nenhum), não publicada no
Marketplace, então o VS Code não a instala sozinho nem por `extensions.json`. Precisa ser aceita
manualmente, uma vez por máquina — `.vscode/settings.json` já associa `*.edl.in` à linguagem `edl`
que ela declara, então o highlight aparece assim que a extensão for reconhecida:

```bash
# Linux nativo
ln -s "$(pwd)/.vscode/extensions/edl" ~/.vscode/extensions/edl-mixr-local

# VS Code Remote (WSL2/SSH) -- o host de extensoes fica do lado remoto/Linux
ln -s "$(pwd)/.vscode/extensions/edl" ~/.vscode-server/extensions/edl-mixr-local
```

Se o VS Code já estava aberto, "Developer: Reload Window" (`Ctrl+Shift+P`) — ela aparece em
Extensões, "Installed", sem ícone/changelog (não tem metadado de Marketplace), mas funcional.
Alternativa sem symlink, empacotando de verdade (precisa de `npm i -g @vscode/vsce`):
`vsce package` dentro de `.vscode/extensions/edl/` e `code --install-extension edl-0.0.1.vsix`.

## 7. Dependências construídas do fonte (`scripts/deps.sh`) — obrigatório para o Groot

`deps/{mixr,behaviortree,jsbsim,openrti,groot}/conanfile.py` são cinco receitas Conan que compilam
essas dependências a partir do fonte; `./scripts/deps.sh` builda as cinco, na ordem certa, Debug e
Release. Para **quatro** delas (`mixr`, `behaviortree.cpp.asa`, `jsbsim`, `openrti`) isto é
**opcional** — por padrão elas vêm prontas do remote Conan privado (seção 4), e `scripts/deps.sh`
só entra em jogo se esse remote não estiver disponível para você. Para o **Groot** é a **única**
forma de tê-lo — não existe pacote pronto em remoto nenhum, público ou privado.

Antes de rodar o script, instale os pacotes de sistema que só o Groot precisa (as outras quatro
receitas não usam nada disto):

```bash
sudo apt install -y cmake qtbase5-dev libqt5svg5-dev libzmq3-dev cppzmq-dev libdw-dev
```

Por que cada um:

- **`cmake`** (≥ 3.2) — a receita builda com `conan.tools.cmake` (`deps/groot/conanfile.py`), que
  invoca o binário `cmake` do **sistema** (não é `tool_requires()` do Conan, não vem por nenhuma
  outra dependência deste pacote): sem ele, `conan create ./deps/groot` falha logo na etapa de
  `configure()` com *"cmake: command not found"*. Confirmado faltando num `ubuntu:24.04` limpo —
  nenhum dos outros quatro pacotes desta lista o traz como dependência transitiva.
- **`qtbase5-dev`**, **`libqt5svg5-dev`** — Groot é uma aplicação Qt5 (interface gráfica + o
  módulo SVG que ele usa para os ícones da árvore); sem eles, o `cmake` da receita falha ao achar
  `Qt5Widgets`/`Qt5Svg`.
- **`libzmq3-dev`** — a camada de transporte do modo Monitor (troca mensagens com uma árvore
  rodando via `PublisherZMQ`, portas 1666/1667 — ver `CLAUDE.md`) — mas só traz a API **C**
  (`zmq.h`).
- **`cppzmq-dev`** — o Groot inclui `zmq.hpp` (`bt_editor/sidepanel_monitor.h`), os *bindings*
  **C++** de libzmq, que são um pacote `apt` **separado** de `libzmq3-dev` — sem ele, o build
  falha na compilação (não no `cmake configure`) com *"zmq.hpp: No such file or directory"*.
  Confirmado faltando num `ubuntu:24.04` limpo — mesma classe de achado do `cmake` acima, só que
  aparece mais tarde (a build chega a compilar boa parte da árvore antes de esbarrar nisto).
- **`libdw-dev`** — símbolos de debug que o build do Groot usa.

Com os pacotes instalados, rode o script (compila as cinco dependências; demora — jsbsim/openrti/
mixr/behaviortree.cpp.asa também são recompiladas do zero):

```bash
./scripts/deps.sh
```

Se você só precisa do Groot (as outras quatro já vêm do remote privado, seção 4), pule o script e
rode só a receita dele:

```bash
conan create ./deps/groot --build=missing --settings=build_type=Release
```

Depois de qualquer um dos dois caminhos, `make open-groot` (na raiz do repositório) resolve o
pacote no cache Conan sozinho e abre o binário — não precisa achar o caminho à mão. Rodar a
interface em si (é uma janela Qt) exige um display de verdade — numa máquina sem X server/Wayland
acessível (container, WSL2 sem WSLg, sessão SSH pura), o build completa normalmente, mas a janela
não abre.

Como usar o Groot para editar/monitorar árvores de comportamento → [`CONTRIBUTING.md`](CONTRIBUTING.md).
