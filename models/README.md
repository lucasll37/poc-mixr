# `models/` — os modelos, como plugins

Um **modelo** é a política da simulação: percepção, decisão, atuação e as classes MIXR próprias
que o cenário nomeia. Ele **não** é compilado dentro do executável — é um `shared_module`
construído numa etapa anterior e aberto com `dlopen` durante o parse do `.epp`.

Isso existe para tornar verificável um cenário concreto: **um terceiro entrega só o binário**, o
host nunca viu o fonte, e mesmo assim tudo funciona.

```
models/
├── flight/          # o modelo de produção das duas pocs gêmeas
│   ├── include/{domain,bt,ubf,xnative}/   src/...   configs/flight_tree.xml
│   ├── data/jsbsim/  # a aeronave (c310) -- é do modelo, não do cenário; as três
│   │                 # pocs (single-thread/multi-thread/bandit-dis) apontam pra cá
│   ├── tests/{domain,tree,native}/        # 76 casos, e nenhum levanta Station
│   ├── docs/ARCHITECTURE.md               # calibração + armadilhas deste modelo
│   ├── Makefile      # build AUTOCONTIDO -- dist/ na raiz deste projeto (§1.1)
│   ├── README.md
│   ├── CHANGELOG.md  # o que mudou, e por quê -- as datas saem do COMMIT, nunca
│   │                 # da mensagem (todo commit deste repo se chama "up")
│   └── meson.build   # UMA árvore, DOIS artefatos (o TC fica atrás de um #ifdef)
├── missile/         # SEGUNDO exemplo de "criar um modelo novo": um único
│   ├── src/xmissile/GuidedMissile.{hpp,cpp}   # Player novo, guiado sobre um
│   │                                          # JSBSimModel anexado -- física
│   │                                          # 6-DOF de verdade
│   ├── src/domain/Guidance.{hpp,cpp}          # lei de guiagem, pura
│   ├── src/plugin.cpp   # publica só "GuidedMissile" -- carregado ao LADO do
│   │                     # flight (2º PluginModule) só no cenário de demo
│   │                     # (src/poc/single-thread/configs/
│   │                     # scenario_missile_demo.epp.in). Ver CLAUDE.md,
│   │                     # seção "Demo: míssil guiado", para o "porque" de
│   │                     # ser um plugin à parte e não dentro do flight.
│   ├── tests/domain/     # domain::pursuit(), puro
│   ├── docs/DESIGN.md
│   ├── Makefile          # build AUTOCONTIDO (§1.1)
│   ├── README.md
│   └── CHANGELOG.md
├── fixtures/
│   └── stub/        # um modelo ESTRANHO, escrito só contra o SDK -- NÃO é um
│       ├── src/stub.cpp             # modelo de produção, é um FIXTURE de
│       │                             # teste (ver §3) E o ponto de partida
│       │                             # para um modelo novo (§2) -- copiar
│       │                             # este diretório já copia as quatro
│       │                             # peças que todo projeto de modelo
│       │                             # deste repositório tem de ter.
│       ├── tests/check_contract.sh  # forma do .so: 1 símbolo T, deps resolvidas
│       ├── docs/CONTRATO.md         # o que um modelo TEM de fazer -- leia primeiro
│       ├── Makefile                 # build AUTOCONTIDO (§1.1)
│       ├── README.md
│       └── CHANGELOG.md
└── plugins/         # DEPÓSITO, não projeto Meson -- um .so de TERCEIRO,
                      # já compilado fora deste repositório, entra aqui e
                      # 'make models' só COPIA pra dist/lib/mixr-plugins/
                      # (nenhum código-fonte, nenhum build). Ver
                      # models/plugins/README.md.
```

**Todo projeto de modelo tem `tests/`, `docs/`, `Makefile`, `README.md` e `CHANGELOG.md` — sem
exceção.** Não é estilo: é o que faz cada um AUTOCONTIDO (§1.1) e verificável sozinho, sem
depender do resto do repositório estar em mente. Quem abre o VS Code só em `models/<nome>/` tem,
ali dentro, como compilar (`Makefile`), como provar que continua certo (`tests/`), o "porquê" das
decisões (`docs/`), a porta de entrada (`README.md`) e o que mudou desde a última vez que olhou
(`CHANGELOG.md`).

**A guarda [`tests/guard/check_modelo_estrutura.sh`](../tests/guard/check_modelo_estrutura.sh)
(suíte `guard`, alvo `modelo-estrutura`) trava isso** — e descobre os projetos por `find`, não por
lista fixa: um modelo novo já nasce cobrado, sem ninguém precisar lembrar de editar o teste.
`models/plugins/` fica de fora de propósito (é depósito de `.so` de terceiro, não projeto).

**Sobre o `CHANGELOG.md` especificamente**, porque a forma dele aqui não é a usual: a versão é a
do `project()` do `meson.build` de cada modelo (o descritor de plugin não carrega versão nenhuma
do modelo — `PluginDescV1` tem `plugin_name`, `mixr_pkg_version` e `build_id`, e mais nada), e as
datas saem da **data de commit**, nunca da mensagem — todo commit deste repositório se chama
`up`.

---

## 1. O build em etapas, e por que a ordem importa

São **quatro projetos Meson**, em quatro diretórios de build. Os modelos são construídos **antes**
do host, mas — desde a decoplagem descrita abaixo — o host **compila** sem depender deles; só
**rodar** algo (`install`/`test`/`run-*`) depende dos modelos já publicados.

```bash
make configure   # 1. conan + meson setup do HOST            -> build/
make sdk         # 2. publica o SDK                          -> dist/{include,lib,lib/pkgconfig}
make models      # 3. flight, missile e stub, cada um autocontido -> models/plugins/ (NUNCA dist/)
make build       # 4. os executáveis do host (só precisa do sdk) -> build/src/poc/<poc>/src/
make install     # 5. sync-plugins (models/plugins/ -> dist/) + instala os binários -> dist/
```

**`make build` NÃO dispara `models` mais** — a cadeia mudou (`build: sdk`, não `build: models`) —
porque compilar o host nunca precisou dos `.so` dos modelos, só de saber onde ficam os headers do
SDK. É `make install` quem encadeia `build` + `sync-plugins` (que por sua vez depende de
`models`). No dia a dia:

```bash
make configure && make build && make install
# ou, direto: make configure && make test (já encadeia install)
```

O único que não entra em nenhuma cadeia é o `configure`: ele roda o Conan e o `meson setup` do
host, e a etapa `sdk` precisa desse `build/` já existir.

| alvo | o que faz | quando rodar |
|---|---|---|
| `make configure` | Conan + `meson setup` do host | uma vez, e depois de `make clean` |
| `make sdk` | compila `xboard`/`xlog`/`xtrack`/`xrlbridge` e instala com `--tags sdk,devel` | raramente sozinho |
| `make models` | configura, compila e **deposita em `models/plugins/`** flight/missile/stub — NUNCA toca `dist/` | ao mexer no modelo |
| `make build` | os três executáveis do host — só depende do `sdk`, não dos modelos | ao mexer no host |
| `make sync-plugins` | copia `models/plugins/*.so` (+ `data/`) para `dist/` — a ÚNICA ponte | raramente sozinho — `install` já chama |
| `make install` | `build` + `sync-plugins` + copia os binários para `dist/bin/` | **necessário** para rodar/testar (ver abaixo) |
| `make test-models` | a suíte do modelo: `domain` + `tree` + `native` (delega pro Makefile de `models/flight`) | ao mexer no modelo |
| `make test` | as **duas** suítes — dispara `test-models` **e** `install` | antes de commitar |
| `make check-plugin-hotswap` | prova que trocar o modelo não recompila a aplicação — depende de `install` | demonstração |
| `make clean` | apaga o `build/`+`dist/` do host, o `build/`+`dist/` de CADA modelo, e os `.so`/`data/` que `make models` gerou em `models/plugins/` | |

> **`make install` (ou `make test`/`run-*`, que já o encadeiam) É NECESSÁRIO para rodar.**
> Diferente do design anterior, `dist/lib/mixr-plugins/` só é populado pelo alvo `sync-plugins`
> (parte de `install`) — nem `make build` nem `make models`, sozinhos, deixam nada executável.
> Isso é deliberado: compilar não deveria depender de onde os artefatos finais moram; só RODAR
> depende disso, e `dlopen()` só acontece em tempo de execução.

> **Depois de um `make clean`, o `-Dtests` volta ao default (`false`)** — o diretório de build foi
> apagado. Para os testes: `meson configure build -Dtests=true` antes do `make build`.

### 1.1 Build autocontido, por modelo

O fluxo acima é **orquestrado** — um só Makefile (o da raiz) sabe os caminhos dos três projetos e
os constrói em sequência. É o fluxo de CI/produção e continua sendo a forma canônica de construir
o repositório inteiro.

**Cada projeto de modelo TAMBÉM tem o próprio `Makefile`, autocontido** — pensado para abrir o VS
Code só naquele diretório (`models/flight/`, `models/missile/` ou `models/fixtures/stub/`) e
iterar sem o resto do repositório em mente:

```bash
# uma vez, na raiz do poc-mixr (publica o que todo modelo consome):
make configure && make sdk

# daqui em diante, de dentro de QUALQUER projeto de modelo:
cd models/flight   # ou models/missile, ou models/fixtures/stub
make               # configura (./build) + compila -> ./dist/lib/mixr-plugins/*.so
make test          # roda a suite DAQUELE modelo, isolada
make install-host  # deposita em ../../models/plugins/ -- so ai um cenario PODE vir a enxergar o .so
```

`./dist` nasce **dentro** do próprio projeto de modelo — é o que "autocontido" quer dizer aqui: o
`.so` (e, no caso do `flight`, também a árvore e a aeronave) sai na raiz do MESMO projeto que você
abriu, sem precisar do Makefile raiz. O único pré-requisito que não tem como deixar de existir é o
SDK publicado pelo host (`make configure && make sdk`, uma vez) — o modelo depende do contrato de
ABI e dos pacotes Conan (`mixr`, `behaviortree.cpp.asa`) que só o projeto host resolve.

`make install-host` é o único alvo que escreve fora do `./dist` local — mas só até
`../../models/plugins/` (lib, flat) e `../../models/plugins/data/<nome>/` (quando há dados), o
MESMO depósito que um `.so` de terceiro usaria (ver `models/plugins/README.md`). **Nunca escreve
em `dist/lib/mixr-plugins/` diretamente** — quem sincroniza `models/plugins/` → `dist/` é o alvo
`sync-plugins` do Makefile raiz, chamado por `make install`. Sem rodar `install-host` E `make
install` (na raiz), o `.so` compilado fica só no projeto do modelo (ou só em `models/plugins/`) —
útil para iterar (compilar, `make test`), inútil para um cenário do host até ser sincronizado até
`dist/`.

Os dois fluxos não competem: `make models` da raiz chama exatamente este `install-host` de cada
um dos três, e continua sendo o que `make build`/`make test`/o CI usam. O Makefile de cada modelo
é para desenvolvimento focado num modelo só — rode `make install-host` (aqui) seguido de `cd
../.. && make install` quando quiser ver o resultado refletido nos cenários do host.

---

## 2. Como criar um modelo novo

**Comece pelo stub, não pelo `flight`.** O `stub` é ~300 linhas e é o exemplo mínimo completo; o
`flight` tem 3.100 e vai te distrair.

```bash
cp -r models/fixtures/stub models/meu-modelo
mv models/meu-modelo/src/stub.cpp models/meu-modelo/src/meu_modelo.cpp
sed -i 's/'"'"'stub'"'"'/'"'"'meu_modelo'"'"'/g' models/meu-modelo/meson.build
sed -i "s|files('src/stub.cpp')|files('src/meu_modelo.cpp')|" models/meu-modelo/meson.build
```

Conferido rodando: essas quatro linhas mais um `meson setup` já produzem um `.so` válido —
**um** símbolo exportado, nenhuma dependência não resolvida.

**A cópia já traz `tests/`, `docs/`, `Makefile`, `README.md` e `CHANGELOG.md`** — as cinco peças
que todo projeto de modelo deste repositório tem de ter (ver o aviso logo abaixo do diagrama, no
topo deste arquivo; a guarda `modelo-estrutura` cobra as cinco). O `CHANGELOG.md` copiado é o do
`stub`: esvazie-o e comece pela versão que o seu `meson.build` declara. Só uma correção é necessária no `Makefile`
copiado: `stub/` mora em `models/fixtures/stub/` (três níveis até a raiz do `poc-mixr`,
`ROOT := $(abspath ../../..)`), enquanto `models/meu-modelo/` mora só dois níveis abaixo — troque
a linha `ROOT := $(abspath ../../..)` para `ROOT := $(abspath ../..)`, ou o Makefile vai apontar
para o diretório **pai** do `poc-mixr` e falhar em `check-root` com um caminho que não existe.
Depois disso, `cd models/meu-modelo && make && make test && make install-host` funciona igual ao
`stub` (ver [fixtures/stub/README.md](fixtures/stub/README.md) para o que cada alvo faz).

### 2.1 O que o `meson.build` tem de ter

Quatro coisas, e nenhuma é opcional:

```meson
sdk_dep = dependency('poc-mixr-sdk', method: 'pkg-config', required: true)

shared_module('meu_modelo',
    files('src/meu_modelo.cpp'),
    cpp_args : [
        '-DMIXR_PLUGIN_PKG_VERSION="' + mixr_dep.version() + '"',
        '-DMIXR_PLUGIN_BUILD_ID="' + cpp.get_id() + ' ' + cpp.version() + '"',
    ],
    link_args             : ['-Wl,--disable-new-dtags', '-Wl,--no-undefined',
                             '-Wl,--exclude-libs,ALL'],
    dependencies          : [mixr_dep, sdk_dep],
    gnu_symbol_visibility : 'hidden',
    install               : true,
    install_dir           : get_option('libdir') / 'mixr-plugins',
    install_rpath         : mixr_libdir + ':' + sdk_libdir + ':$ORIGIN/..',
)
```

1. **`shared_module()`**, nunca `library()` — o artefato não pode ser linkável, ou alguém acaba
   pondo num `link_with:` e o processo fica com duas cópias dos `MetaObject` das suas classes.
2. **`gnu_symbol_visibility: 'hidden'`** + **`-Wl,--exclude-libs,ALL`** — a segunda é obrigatória
   se você linkar **qualquer biblioteca estática**: a visibilidade escondida **não se aplica a
   objetos vindos de um `.a`**. A BehaviorTree.CPP deste pacote traz 447 símbolos globais.
3. **`-Wl,--no-undefined`** — o executável não exporta símbolo nenhum, então um modelo não pode
   chamar código da aplicação. Esta flag transforma isso em erro de **link**, não de `dlopen`.
4. **`dependencies` só pode ter `mixr_dep`, `sdk_dep`** e, se precisar, `behavior_tree_dep`.
   **Nunca** as libs de `shared/` que são estáticas (`xtacview`, `xclock`, `xjoystick`, `xmsg`) —
   você ganharia uma cópia privada dos estáticos delas.

E acrescente o alvo ao `models:` do [Makefile](../Makefile), no molde do `stub`.

### 2.2 O que o `.cpp` tem de ter

Leia **[fixtures/stub/docs/CONTRATO.md](fixtures/stub/docs/CONTRATO.md)** — é a lista completa. O resumo:

- exportar o ponto de entrada **sempre pela macro** `MIXR_PLUGIN_DEFINE`, nunca escrevendo a
  assinatura à mão (num alvo com visibilidade escondida ela viraria símbolo invisível ao `dlsym`,
  e o sintoma aparece longe);
- registrar os nomes de fábrica que **o cenário nomeia**, derivando das classes base certas;
- **escrever no `xboard`** — a única obrigação que falha em **silêncio**. Sem ela o dump sai com
  `bt=--` e `dec=0`, sem erro, com todos os outros testes verdes.

### 2.3 Verificar

```bash
make models
nm -D --defined-only dist/lib/mixr-plugins/libmeu_modelo.so | grep ' T '   # tem de ser 1 linha
ldd dist/lib/mixr-plugins/libmeu_modelo.so | grep 'not found'             # tem de ser vazio
```

Depois aponte um cenário para ele (§4) e rode.

---

## 3. Para que serve o `stub` (e por que mora em `fixtures/`)

`fixtures/` já diz o principal: isto **não é um terceiro modelo de produção** ao lado de `flight` e
`missile` — é um fixture de teste, com dois papéis, e o segundo é o que justifica existir.

### Papel 1 — o exemplo mínimo

~300 linhas, projeto Meson próprio, e a superfície inteira que ele conhece são o SDK e o MIXR.
É o ponto de partida para um modelo novo (§2).

### Papel 2 — a prova de que o contrato basta

Todos os outros testes de plugin carregam **o mesmo** modelo, compilado do mesmo fonte. Nenhum
deles pode falhar pelo motivo que importa — *"um `.so` que eu não escrevi não serve"*.

O teste `plugin-modelo-estranho` roda o cenário de **produção** contra o stub trocando **apenas o
`file:`** do `( PluginModule )`. A edição mínima é parte da asserção: se fosse preciso mexer em
mais alguma coisa, é isso que o teste teria de denunciar.

```bash
meson configure build -Dtests=true && make build
meson test -C build --suite plugin        # inclui plugin-modelo-estranho
```

**O modo de falha que só ele pega:** um modelo que responde pelos nomes certos, deriva das bases
certas, mas nunca chama o `xboard`. O host sobe, o cenário parseia, os aviões voam pelo
`Autopilot` nativo — e o dump sai vazio, com `plugin-contrato`, `plugin-simbolo`,
`plugin-negativos` e as guardas **todos verdes**.

**Ele é uma especificação executável.** Se você acrescentar uma classe ou um slot ao cenário, o
stub quebra — e isso é bom: vira o lembrete de atualizar o `CONTRATO.md` na hora, em vez de virar
surpresa do terceiro.

> Quando criar um modelo novo com classes novas, **atualize o stub junto**. Ele existe para
> quebrar.

---

## 4. Como criar uma poc nova que usa um modelo novo

A poc é o **host**: `main.cpp`, `mixr_factory.cpp` e os módulos de `app/`. Nada de modelo.
`src/poc/bandit-dis/` é o exemplo mais enxuto — ele nunca teve modelo nenhum.

1. **`src/poc/<nome>/`** seguindo o molde de `src/poc/single-thread/` (só `app/`, `main.cpp`,
   `mixr_factory.{hpp,cpp}`), e `subdir('./<nome>')` em [src/meson.build](../src/meson.build).
2. **`mixr_factory.cpp`** encadeia `mixr::xplugin::factory` (as classes EDL `( PluginLoader )` e
   `( PluginModule )`) e, no fim, `mixr::xplugin::loadedFactory` — as classes que vieram do
   plugin. Copie de `src/poc/bandit-dis/src/mixr_factory.cpp`.
3. **`app/StationBuilder.cpp`** chama `xplugin::setBuiltinFactory(mixrFactoryBuiltin)` antes do
   `edl_parser` e `xplugin::seal()` depois.
4. **`dependencies`** do `executable()`: `[thread_dep, mixr_dep, xtacview_dep, xclock_dep,
   xjoystick_dep, xlog_dep, xboard_dep, xtrack_dep, xplugin_dep, xmsg_dep]` — **sem**
   `behavior_tree_dep` (quem decide é o modelo).
5. **`install_rpath`**: `mixr_libdir + ':' + own_libs_rpath` — o `$ORIGIN/../lib` é o que faz o
   binário de `dist/bin/` achar a `libxboard.so` em `dist/lib/`.
6. **No cenário**, o bloco de plugin como **primeira entrada de `components:`**:

```
   components: {
      plugins: ( PluginLoader
         searchPaths: { "./dist/lib/mixr-plugins/" }
         modules: {
            ( PluginModule  file: "libmeu_modelo.so"
               provides: { AlertDatalink TacticalAlert FlightState
                           BtBehavior AltitudeSafetyBehavior FlightAction } )
         }
      )
      ...
```

> **O bloco tem de vir ANTES do primeiro uso**, e essa é a única regra que o autor do cenário
> precisa lembrar. O motivo é mecânico: a produção `arglist` do `edl_parser` é recursiva à
> esquerda, então formas irmãs são construídas **na ordem do texto**, e a carga acontece no
> `isValid()` do `( PluginLoader )`. Fora de ordem, o `mixrFactory` aborta explicando isso — não
> há silêncio nem SIGSEGV.

> **`provides:` é igualdade EXATA de conjunto.** Se a `.so` não entregar exatamente esses nomes, o
> processo morre dizendo o que ela entrega. É o que pega uma `.so` velha esquecida no caminho.

---

## 5. Armadilhas confirmadas — não redescobrir

1. **`meson compile` resolve alvo por NOME; o `ninja` cru, por caminho de saída.**
   `ninja -C build xboard` dá *"unknown target"*.
2. **`meson install --tags sdk` NÃO instala os headers.** `install_headers()` não aceita
   `install_tag` no Meson 1.2, então eles ficam com a tag automática `devel`. Tem de ser
   `--tags sdk,devel`, e o sintoma da falta aparece dois alvos depois.
3. **`PKG_CONFIG_PATH` de ambiente é descartado** quando o native-file do Conan fixa
   `pkg_config_path`. Use `-Dpkg_config_path=` na linha de comando, e o separador é **vírgula**.
4. **`meson test` devolve `rc=0` para suíte vazia** ("No tests defined."). Por isso `make test` e
   `make test-models` conferem a contagem com `meson introspect --tests` antes de rodar.
5. **`meson configure -Dasan=true` dispara um regenerate que reavalia dependências** — e ali o
   `dependency('poc-mixr-sdk')` já falhou. Use a linha completa de `meson setup --reconfigure`
   (é o que o alvo `models` faz, e o `test-asan` reusa com `ASAN=true`).
6. **Nunca `dlclose`.** Toda instância viva guarda ponteiro para dentro do `.so`, e o destrutor
   **escreve** lá. *"Sem recompilar tudo"* — sim; *"sem reiniciar o processo"* — **não**.
7. **O `ROOT` do Makefile autocontido (§1.1) é a profundidade do diretório, não um valor
   universal.** `models/flight/` e `models/missile/` ficam dois níveis abaixo da raiz
   (`ROOT := $(abspath ../..)`); `models/fixtures/stub/` fica três (`../../..`). Copiar o
   `Makefile` do `stub` para um modelo novo direto sob `models/` (o caminho recomendado em §2) e
   esquecer de tirar um `../` faz `check-root` apontar para o diretório **pai** do `poc-mixr` e
   falhar dizendo que o SDK não existe — mesmo com ele publicado.

---

## Ler também

- **[fixtures/stub/docs/CONTRATO.md](fixtures/stub/docs/CONTRATO.md)** — o que um modelo TEM de fazer
- **[fixtures/stub/README.md](fixtures/stub/README.md)** — o build autocontido do fixture/template,
  alvo por alvo (`make`, `make test`, `make install-host`)
- **[flight/docs/ARCHITECTURE.md](flight/docs/ARCHITECTURE.md)** — calibração e armadilhas do
  modelo de produção
- **[missile/docs/DESIGN.md](missile/docs/DESIGN.md)** — a lei de guiagem da demo de míssil
- **[../shared/xplugin/README.md](../shared/xplugin/README.md)** — o contrato de ABI e a seção
  **Limites**, que diz o que ele **não** garante
- **[../tests/README.md](../tests/README.md)** — as duas suítes e o que cada camada prova
- **[../src/poc/single-thread/README.md](../src/poc/single-thread/README.md)** — a dissecação profunda do
  modelo de produção (os arquivos moraram para cá, o texto continua valendo)
