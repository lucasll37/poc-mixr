# Instalação — `models/player/A4`, isolado

Este projeto de modelo é **autocontido** (ver [`README.md`](README.md), seção "Build
autocontido"): fora de um único pré-requisito que não tem como deixar de existir — o SDK publicado
pelo projeto HOST, uma vez, na raiz do `poc-mixr` — dá para abrir o VS Code só nesta pasta,
compilar, testar e editar a árvore de comportamento sem o resto do repositório em mente. Este
roteiro cobre exatamente esses dois pedaços: os pacotes de sistema que este projeto usa, e o passo
único que amarra este diretório ao resto do repositório.

## 1. Pacotes de sistema

Este projeto compila com a MESMA toolchain do host — passo a passo completo, com o "porquê" de
cada pacote (inclusive por que o remote público do Conan às vezes recompila meio mundo do fonte em
GCC 13) → [`../../../INSTALL.md`](../../../INSTALL.md) §1. Resumo do que **este** projeto
especificamente usa:

| ferramenta | por quê |
|---|---|
| GCC ≥ 7 ou Clang ≥ 5, C++17 | compila `domain/`, `bt/`, `ubf/`, `xnative/` |
| Meson ≥ 1.0, Ninja, pkg-config | build deste projeto e resolução do SDK/MIXR/BT.CPP via `.pc` |
| Conan ≥ 2.0 | resolve `mixr`/`behaviortree.cpp.asa` — pelos MESMOS pacotes que o host já resolveu, não uma cópia separada |
| Python 3 | `tools/sync_tree_models.py` e a suíte de testes (`meson test` chama scripts Python em alguns alvos) |

```bash
sudo apt install -y build-essential ninja-build meson pkg-config python3
```

Conan em si (`pipx install conan`, perfil, remote privado) só precisa existir uma vez por máquina,
não uma vez por projeto — se a raiz do `poc-mixr` já rodou `make configure` com sucesso, os passos
2-4 do [`INSTALL.md`](../../../INSTALL.md) da raiz já estão feitos e podem ser pulados aqui.

## 2. O pré-requisito que não some: o SDK do host

Este projeto **não sabe compilar sozinho** — ele consome, via `pkg-config`, o contrato de ABI de
plugin (`poc-mixr-sdk.pc`) e os pacotes Conan (`mixr`, `behaviortree.cpp.asa`) que só o projeto
HOST resolve. É a única coisa que amarra este diretório ao resto do repositório, e não tem como
deixar de existir: um modelo é um `.so` compilado contra o layout de memória exato que o host
publica (ver `libs/xplugin/PluginAbi.hpp`), então algo tem que publicar esse contrato antes.

```bash
cd ../../..                    # a raiz do poc-mixr
make configure && make sdk     # uma vez -- publica build/conan_meson_native.ini e dist/lib/pkgconfig/
cd models/player/A4            # de volta aqui
```

Sem isso, `check-root` — rodado automaticamente por todo alvo deste `Makefile` (`configure`,
`build`, `test`, ...) — recusa antes de tentar compilar, apontando exatamente qual dos dois
arquivos falta:

```
faltando build/conan_meson_native.ini
  rode 'cd <raiz> && make configure' primeiro (uma vez).
```
ou
```
faltando o SDK de plugin em <raiz>/dist
  rode 'cd <raiz> && make sdk' primeiro (uma vez).
```

Depois de publicado uma vez, esta etapa não precisa se repetir — só de novo se a raiz rodar `make
clean` (que apaga `build/`/`dist/` do host) ou se você quiser testar contra uma versão diferente
do SDK. Instalação completa do host (Conan, remote privado com `mixr`/`behaviortree.cpp.asa`,
perfil) → [`../../../INSTALL.md`](../../../INSTALL.md).

## 3. Compilar e testar, só daqui

Com os dois passos acima feitos, o resto roda inteiramente dentro desta pasta:

```bash
make                 # configura (./build) + compila -> ./dist/lib/mixr-plugins/{libflight,libflight_tc}.so
make test             # domain (47) + tree (20, incluindo tree-model-sync) + native (9) = 76+1 casos
make install-host     # deposita em ../../../plugins/ -- o UNICO alvo que escreve fora daqui
```

`make help` lista todos os alvos, com descrição. Ver [`README.md`](README.md), seção "Build
autocontido", para o que cada um faz e por que `install-host` nunca escreve direto em `dist/` da
raiz (quem sincroniza `plugins/` → `dist/` é `make install`, na raiz — ver
[`../README.md`](../../README.md) §1).

## 4. Opcional: Groot, para editar/monitorar a árvore de comportamento

Só necessário se você for editar `configs/flight_tree*.xml` visualmente ou monitorar uma árvore ao
vivo dentro da simulação — compilar/testar/publicar este modelo (seções 1-3) não depende disso.
Diferente do resto deste roteiro, o Groot **não tem `Makefile` neste projeto**: é uma ferramenta do
repositório inteiro (compartilhada por todos os modelos), não deste modelo especificamente —
instala-se e abre-se sempre a partir da raiz:

```bash
sudo apt install -y cmake qtbase5-dev libqt5svg5-dev libzmq3-dev cppzmq-dev libdw-dev
cd ../../..                                                              # a raiz do poc-mixr
conan create ./deps/groot --build=missing --settings=build_type=Release # uma vez
make open-groot                                                          # sempre que quiser abrir
```

O Groot não tem pacote pronto em remoto nenhum (público ou privado) — compilar do fonte é a única
forma de tê-lo; `conan create ./deps/groot ...` é a receita mínima quando as outras quatro
dependências (`mixr`, `behaviortree.cpp.asa`, `jsbsim`, `openrti`) já vêm prontas do remote
privado (o caminho normal). Detalhe de cada pacote de sistema e a alternativa
`./scripts/deps.sh` (que builda as cinco receitas do zero, incluindo essas quatro) →
[`../../../INSTALL.md`](../../../INSTALL.md) §7.

Como usar o Groot com a árvore DESTE modelo — abrir a árvore de produção, registrar um nó novo e
resincronizar os 5 `.xml` com `tools/dump_tree_model.cpp`, criar uma árvore experimental,
monitorar ao vivo com `MIXR_GROOT_MONITOR` → [`CONTRIBUTING.md`](CONTRIBUTING.md).

## 5. Checagem final

```bash
gcc --version && meson --version && ninja --version && pkg-config --version && python3 --version
test -f ../../../dist/lib/pkgconfig/poc-mixr-sdk.pc && echo "SDK do host: OK"
```

Com os cinco respondendo e o SDK presente, `make && make test` (aqui dentro) deve rodar do início
ao fim sem tocar em nada fora desta pasta além da leitura do SDK publicado.
