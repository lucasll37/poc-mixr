# Instalação — passo a passo a partir de uma máquina limpa

Este roteiro é o que [`.gitlab-ci.yml`](.gitlab-ci.yml) segue, passo a passo, contra um
`ubuntu:24.04` recém-saído da instalação — o job `build` roda exatamente esta sequência antes de
`make configure`. `make test-ci` (ver [`README.md`](README.md), seção "CI (GitLab)") roda esse
pipeline localmente, num container Docker, do zero; se algum passo aqui parecer incompleto, é ali
que a lacuna aparece primeiro.

`make test-ci` é opt-in e exige **Docker** instalado à parte — esse sim só é pré-requisito desta
checagem, não do build: [docker.com](https://www.docker.com/). **Node.js é pré-requisito do
projeto** e tem seção própria (§5).

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

## 4. Dependências construídas do fonte (`scripts/deps.sh`)

`deps/{mixr,behaviortree,jsbsim,openrti,groot}/conanfile.py` são cinco receitas Conan que compilam
essas dependências a partir do fonte — `mixr` e `behaviortree.cpp.asa` são os dois frameworks C++
que este projeto usa (ver glossário no [`README.md`](README.md)), `jsbsim` é o motor de física de
voo (ver o mesmo glossário), `openrti` é uma implementação de RTI (*Runtime Infrastructure*) para
HLA (*High Level Architecture*, padrão IEEE 1516) que o MIXR declara mas
este fork não compila (a interoperabilidade usada aqui é DIS), e `groot` é o editor/monitor visual
das árvores de comportamento; `./scripts/deps.sh` builda as cinco, na ordem certa, Debug e
Release.

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

> **Duas dessas receitas aplicam CORREÇÕES DE FONTE, e um pacote em cache anterior a elas
> reintroduz o bug em silêncio.** Ao mexer em `deps/groot/conanfile.py` ou
> `deps/behaviortree/conanfile.py`, **remova o pacote antes de recriar** — só assim a `source()`
> (que é quem aplica os patches) roda de novo:
>
> ```bash
> conan remove 'groot/*' -c && conan create ./deps/groot --build=missing --settings=build_type=Release
> ```
>
> - **`groot` — FIX 6**: sem ela, o **modo Monitor fecha a janela sozinho**, sem diálogo e sem
>   mensagem, poucos milissegundos depois de conectar (`std::out_of_range` escapando de um slot Qt).
>   Detalhe completo em [`CLAUDE.md`](CLAUDE.md), seção "Groot", armadilha nº3. `scripts/find_groot.sh`
>   avisa em `stderr` quando o binário em cache é anterior à correção — ele procura o marcador
>   `POC-MIXR-FIX6`, que fica em `lib/libbehavior_tree_editor.so` (é lá que
>   `sidepanel_monitor.cpp` compila), não em `bin/Groot`.
> - **`behaviortree.cpp.asa`**: corrige um *use-after-free* no destrutor de `BT::PublisherZMQ`
>   (confirmado com AddressSanitizer). Como o patch é aplicado na `source()` desta receita, ele
>   existe **só em pacote construído do fonte** — que é justamente o caminho desta seção e o que o
>   CI usa, então é o comportamento corrente. Um pacote **em cache** anterior ao patch o perde em
>   silêncio: `conan remove 'behaviortree.cpp.asa/*' -c` antes de recriar.
>
> **Se o Groot fechar sozinho, olhe `build/groot.log`**: `make open-groot` grava ali a saída dele
> (antes ia para `/dev/null`, e era por isso que não havia onde procurar). `make open-groot FG=1`
> roda em primeiro plano.

## 5. Node.js

**Pré-requisito do projeto, não opcional** — mesma natureza do Groot (§4): não é dependência de
**build** (o `meson`/`ninja` do host e dos modelos nunca o invocam, e `make configure`/`build`/
`models`/`install`/`test` rodam sem ele), mas é o que faz funcionar o ferramental documentado do
repositório. Três alvos o exigem:

| alvo | por que precisa de Node |
|---|---|
| `make docs` | regenera `docs/manual/index.html` a partir de `doc.jsx`. **O HTML gerado é versionado**, então `make open-docs` abre a página já pronta sem Node nenhum — só *regenerar* exige |
| `make open-edl-builder` | recompila e abre `src/ui/edl-builder.html` (o editor visual de cenário EDL). Aqui não há saída equivalente versionada: sem Node o alvo não roda |
| `make test-ci` | chama `npx gitlab-ci-local`, que roda o pipeline do `.gitlab-ci.yml` num container. O Node é exigido na **máquina host** (é lá que o `npx` roda) e **também dentro do container**, onde o pipeline o instala do zero seguindo exatamente esta seção — é assim que este §5 fica coberto por processo automatizado. Também precisa de Docker (ver [`README.md`](README.md)) |

**Versão mínima: 18** — e o pacote da distro pode não servir. Medido nesta base de código: o `apt`
do Ubuntu 22.04 oferece `nodejs 12.22.9`, bem abaixo do mínimo. Confira antes de assumir que o
pacote da sua distro serve:

```bash
apt-cache policy nodejs
```

O caminho é o repositório **NodeSource**, que instala `nodejs` pelo `apt`, system-wide, para todo
usuário da máquina. `setup_24.x` fixa a linha **24 "Krypton"**, a LTS ativa:

```bash
curl -fsSL https://deb.nodesource.com/setup_24.x | sudo -E bash -
sudo apt install -y nodejs
sudo npm install -g npm@latest
node --version && npm --version
```

**A primeira execução de cada um dos dois precisa de rede** — `cdnjs.cloudflare.com` (React e
ReactDOM 18 UMD, baixados com `curl`) e `registry.npmjs.org` (o Babel, via `npm`). Depois disso
fica tudo em `docs/manual/.cache/` e `src/ui/.cache/` — um cache por ferramenta, não
compartilhados, os dois gitignorados — e os alvos rodam offline. As **páginas geradas** nunca
precisam de rede para abrir.

> **Não confundir com `make run-node`.** `dist/bin/node` é um binário **deste projeto** — o runner
> headless de um cenário, escrito em C++, sem TUI (ver [`src/node/README.md`](src/node/README.md))
> — e não tem relação nenhuma com o Node.js desta seção. A colisão de nome é infeliz, e só isso.

## 6. Checagem

```bash
gcc --version && meson --version && ninja --version && pkg-config --version && conan --version \
  && node --version && npm --version
```

## 7. Editor: VS Code (opcional)

### 7.1. `clangd` (C++)

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

### 7.2. Extensões recomendadas

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
| `llvm-vs-code-extensions.vscode-clangd` | o language server C++ deste projeto — ver §7.1 |
| `ms-vscode.cpptools` | debugger (`cppdbg`) e tarefas de build da Microsoft — **não** o IntelliSense dela, que `.vscode/settings.json` já desliga (`C_Cpp.intelliSenseEngine: "disabled"`) a favor do clangd |
| `spencerwmiles.vscode-task-buttons` | mostra o botão "$(play) app" na barra de status (task "Run app" de `.vscode/tasks.json`, que roda `./build/app/src/app -folder ./sandbox`) — **essa task assume `gnome-terminal` instalado** (abre o app num terminal externo); em KDE/XFCE/WSL2 sem esse pacote a task falha com "command not found" — rode o binário direto num terminal seu nesse caso |
| `anthropic.claude-code` | a extensão do Claude Code em si |
| `ms-toolsai.jupyter` | notebooks `.ipynb`, se usados em `src/poc/rl-training/` |
| `yzhang.markdown-all-in-one` | edição confortável dos muitos `.md` deste repositório |
| `pkief.material-icon-theme`, `natqe.reload` | cosméticas/conveniência, sem efeito no build |


### 7.3. Highlight de `.edl` (extensão local, não vem do Marketplace)

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