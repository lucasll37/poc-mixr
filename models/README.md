# `models/` — os modelos, como plugins

Um **modelo** é a política da simulação: percepção, decisão, atuação e as classes MIXR próprias
que o cenário nomeia. Ele **não** é compilado dentro do executável — é um `shared_module`
construído numa etapa anterior e aberto com `dlopen` durante o parse do `.edl`.

Isso existe para tornar verificável um cenário concreto: **um terceiro entrega só o binário**, o
host nunca viu o fonte, e mesmo assim tudo funciona.

> **Quem já existe, quem cuida:** [`REGISTRO.md`](REGISTRO.md) é a tabela de coordenação entre
> devs — antes de começar um modelo novo (ex.: os da fila em `TODO.md`), confira lá se alguém já
> não começou o mesmo.

```
models/
├── events/          # o contrato de eventos que atravessam fronteira de plugin --
│   │                 # payload (base::Object-derivada) + token (EventTokens.hpp).
│   │                 # shared_library() de verdade (dlopen exige isso, ver o
│   │                 # cabecalho de events/meson.build) -- publicada pelo MESMO
│   │                 # SDK que xboard/xlog/xtrack, so o endereco do fonte muda.
│   │                 # Nao e um modelo -- e' consumida POR eles (e por app/).
│   └── payloads/EID_ALERT/TacticalAlert.{hpp,cpp}   # um payload, uma pasta,
│                     # nomeada igual ao token que carrega (ver EventTokens.hpp)
├── player/          # os projetos de modelo -- cada um um .so de PRODUCAO
│   │                # (exceto fixtures/stub e template/, ver abaixo)
│   ├── A4/              # o modelo de produção das pocs de DIS (nome de fábrica/
│   │   │                # biblioteca continuam "flight" -- libflight.so/
│   │   │                # libflight_tc.so; só o título da PASTA é o A4)
│   │   ├── include/{domain,bt,ubf,xnative}/   src/...   configs/flight_tree.xml
│   │   ├── data/jsbsim/  # a aeronave (c310) -- é do modelo, não do cenário; as três
│   │   │                 # pocs (single-thread/multi-thread/bandit) apontam pra cá
│   │   ├── tests/{domain,tree,native}/        # 76 casos, e nenhum levanta Station
│   │   ├── docs/ARCHITECTURE.md               # calibração + armadilhas deste modelo
│   │   ├── Makefile      # build AUTOCONTIDO -- dist/ na raiz deste projeto (§1.1)
│   │   ├── README.md
│   │   ├── CHANGELOG.md  # o que mudou, e por quê -- as datas saem do COMMIT, nunca
│   │   │                 # da mensagem (todo commit deste repo se chama "up")
│   │   └── meson.build   # UMA árvore, DOIS artefatos (o TC fica atrás de um #ifdef)
│   ├── missile/         # SEGUNDO exemplo de "criar um modelo novo": um único
│   │   ├── src/xmissile/GuidedMissile.{hpp,cpp}   # Player novo, guiado sobre um
│   │   │                                          # JSBSimModel anexado -- física
│   │   │                                          # 6-DOF de verdade
│   │   ├── src/domain/Guidance.{hpp,cpp}          # lei de guiagem, pura
│   │   ├── src/plugin.cpp   # publica só "GuidedMissile" -- carregado ao LADO do
│   │   │                     # flight (2º PluginModule) só no cenário de demo
│   │   │                     # (src/poc/dis/single-thread/configs/
│   │   │                     # scenario_missile_demo.edl.in). Ver CLAUDE.md,
│   │   │                     # seção "Demo: míssil guiado", para o "porque" de
│   │   │                     # ser um plugin à parte e não dentro do flight.
│   │   ├── tests/domain/     # domain::pursuit(), puro
│   │   ├── docs/DESIGN.md
│   │   ├── Makefile          # build AUTOCONTIDO (§1.1)
│   │   ├── README.md
│   │   └── CHANGELOG.md
│   └── fixtures/
│       └── stub/        # um modelo ESTRANHO, escrito só contra o SDK -- NÃO é um
│           ├── src/stub.cpp             # modelo de produção, é um FIXTURE de
│           │                             # teste (ver §3) E o ponto de partida
│           │                             # para um modelo novo (§2) -- copiar
│           │                             # este diretório já copia as quatro
│           │                             # peças que todo projeto de modelo
│           │                             # deste repositório tem de ter.
│           ├── tests/check_contract.sh  # forma do .so: 1 símbolo T, deps resolvidas
│           ├── docs/CONTRATO.md         # o que um modelo TEM de fazer -- leia primeiro
│           ├── Makefile                 # build AUTOCONTIDO (§1.1)
│           ├── README.md
│           └── CHANGELOG.md
├── template/        # o OUTRO ponto de partida copiável (§2.4) -- ao contrário do
│   │                # stub (achatado, um arquivo, prova que o contrato BASTA),
│   │                # mostra a separação em CAMADAS (domain/ubf/xnative) com uma
│   │                # única decisão de exemplo. NÃO é produção: nenhum cenário
│   │                # aponta para ele, e ele NUNCA entra no alvo `models:` abaixo.
│   ├── include/{domain,ubf,xnative}/   src/...
│   ├── tests/domain/test_ExampleThreshold.cpp   tests/check_contract.sh
│   ├── docs/ARCHITECTURE.md   # as camadas, o porquê, quando crescer p/ BehaviorTree.CPP
│   ├── docs/PRIMEIROS-PASSOS.md   # o roteiro de copiar isto e virar um modelo com nome próprio
│   ├── Makefile          # build AUTOCONTIDO (§1.1)
│   ├── README.md
│   └── CHANGELOG.md
└── README.md        # este arquivo
```

`plugins/` (o depósito de `.so`, próprio ou de terceiro) **não mora aqui** — fica na raiz do
repositório, irmão de `dist/`/`build/`, porque nunca foi um projeto Meson (ver
`../plugins/README.md`).

**Todo projeto de modelo tem `tests/`, `docs/`, `Makefile`, `README.md` e `CHANGELOG.md` — sem
exceção.** Não é estilo: é o que faz cada um AUTOCONTIDO (§1.1) e verificável sozinho, sem
depender do resto do repositório estar em mente. Quem abre o VS Code só em `models/<nome>/` tem,
ali dentro, como compilar (`Makefile`), como provar que continua certo (`tests/`), o "porquê" das
decisões (`docs/`), a porta de entrada (`README.md`) e o que mudou desde a última vez que olhou
(`CHANGELOG.md`).

**A guarda [`tests/guard/check_modelo_estrutura.sh`](../tests/guard/check_modelo_estrutura.sh)
(suíte `guard`, alvo `modelo-estrutura`) trava isso** — e descobre os projetos por `find`, não por
lista fixa: um modelo novo já nasce cobrado, sem ninguém precisar lembrar de editar o teste.
`plugins/` fica de fora de propósito (é depósito de `.so` de terceiro, não projeto).

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
make models      # 3. flight, missile e stub, cada um autocontido -> plugins/ (NUNCA dist/)
make build       # 4. os executáveis do host (só precisa do sdk) -> build/src/poc/<poc>/src/
make install     # 5. sync-plugins (plugins/ -> dist/) + instala os binários -> dist/
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
| `make models` | configura, compila e **deposita em `plugins/`** flight/missile/stub — NUNCA toca `dist/` | ao mexer no modelo |
| `make build` | os três executáveis do host — só depende do `sdk`, não dos modelos | ao mexer no host |
| `make sync-plugins` | copia `plugins/*.so` (+ `data/`) para `dist/` — a ÚNICA ponte | raramente sozinho — `install` já chama |
| `make install` | `build` + `sync-plugins` + copia os binários para `dist/bin/` | **necessário** para rodar/testar (ver abaixo) |
| `make test-models` | a suíte do modelo: `domain` + `tree` + `native` (delega pro Makefile de `models/player/A4`) | ao mexer no modelo |
| `make test` | as **duas** suítes — dispara `test-models` **e** `install` | antes de commitar |
| `make check-plugin-hotswap` | prova que trocar o modelo não recompila a aplicação — depende de `install` | demonstração |
| `make clean` | apaga o `build/`+`dist/` do host, o `build/`+`dist/` de CADA modelo, e os `.so`/`data/` que `make models` gerou em `plugins/` | |

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
Code só naquele diretório (`models/player/A4/`, `models/player/missile/` ou `models/player/fixtures/stub/`) e
iterar sem o resto do repositório em mente:

```bash
# uma vez, na raiz do poc-mixr (publica o que todo modelo consome):
make configure && make sdk

# daqui em diante, de dentro de QUALQUER projeto de modelo:
cd models/player/A4   # ou models/player/missile, ou models/player/fixtures/stub
make               # configura (./build) + compila -> ./dist/lib/mixr-plugins/*.so
make test          # roda a suite DAQUELE modelo, isolada
make install-host  # deposita em ../../plugins/ -- so ai um cenario PODE vir a enxergar o .so
```

`./dist` nasce **dentro** do próprio projeto de modelo — é o que "autocontido" quer dizer aqui: o
`.so` (e, no caso do `flight`, também a árvore e a aeronave) sai na raiz do MESMO projeto que você
abriu, sem precisar do Makefile raiz. O único pré-requisito que não tem como deixar de existir é o
SDK publicado pelo host (`make configure && make sdk`, uma vez) — o modelo depende do contrato de
ABI e dos pacotes Conan (`mixr`, `behaviortree.cpp.asa`) que só o projeto host resolve.

`make install-host` é o único alvo que escreve fora do `./dist` local — mas só até
`../../plugins/` (lib, flat) e `../../plugins/data/<nome>/` (quando há dados), o
MESMO depósito que um `.so` de terceiro usaria (ver `plugins/README.md`). **Nunca escreve
em `dist/lib/mixr-plugins/` diretamente** — quem sincroniza `plugins/` → `dist/` é o alvo
`sync-plugins` do Makefile raiz, chamado por `make install`. Sem rodar `install-host` E `make
install` (na raiz), o `.so` compilado fica só no projeto do modelo (ou só em `plugins/`) —
útil para iterar (compilar, `make test`), inútil para um cenário do host até ser sincronizado até
`dist/`.

Os dois fluxos não competem: `make models` da raiz chama exatamente este `install-host` de cada
um dos três, e continua sendo o que `make build`/`make test`/o CI usam. O Makefile de cada modelo
é para desenvolvimento focado num modelo só — rode `make install-host` (aqui) seguido de `cd
../.. && make install` quando quiser ver o resultado refletido nos cenários do host.

---

## 2. Como criar um modelo novo

**O caminho recomendado é o gerador automático**, não a cópia manual abaixo:

```bash
make new-model NAME=meu_modelo KIND=stub   # ou KIND=template
```

Ele automatiza exatamente a receita mecânica documentada nesta seção — copia o ponto de partida,
renomeia projeto/módulo/namespace nos quatro lugares que têm que concordar (§2.1), calcula a linha
`ROOT` do Makefile pela profundidade real do destino (em vez de copiá-la, a armadilha mais citada
da seção 5), esvazia o `CHANGELOG.md` e roda `make test install` de verificação antes de devolver
ao terminal — terminando com um checklist do que só um humano pode terminar (a lógica de domínio,
o `provides:` do `.edl`, `git add`). Não decide **nada** sozinho: não escreve lógica de negócio,
não registra o modelo em cenário nenhum, não comita.

**Comece pelo stub, não pelo `flight`.** O `stub` é ~300 linhas e é o exemplo mínimo completo; o
`flight` (a pasta é `A4/`, o nome de fábrica/biblioteca continua `flight`) tem 3.100 linhas e vai
te distrair. Isto vale para o caso em que o seu modelo decide com **uma** coisa (uma regra, uma
condição) — se você já sabe que vai coordenar mais de uma decisão e prefere nascer separado em
camadas (`domain/`→`ubf/`→`xnative/`) em vez de um arquivo achatado, o ponto de partida é
[`player/template/`](player/template/README.md) em vez do `stub` — ver §2.4.

### O que o gerador faz por baixo dos panos (receita manual, se preferir não usá-lo)

```bash
cp -r models/player/fixtures/stub models/player/meu-modelo
mv models/player/meu-modelo/src/stub.cpp models/player/meu-modelo/src/meu_modelo.cpp
sed -i 's/'"'"'stub'"'"'/'"'"'meu_modelo'"'"'/g' models/player/meu-modelo/meson.build
sed -i "s|files('src/stub.cpp')|files('src/meu_modelo.cpp')|" models/player/meu-modelo/meson.build
```

Conferido rodando: essas quatro linhas mais um `meson setup` já produzem um `.so` válido —
**um** símbolo exportado, nenhuma dependência não resolvida.

**A cópia já traz `tests/`, `docs/`, `Makefile`, `README.md` e `CHANGELOG.md`** — as cinco peças
que todo projeto de modelo deste repositório tem de ter (ver o aviso logo abaixo do diagrama, no
topo deste arquivo; a guarda `modelo-estrutura` cobra as cinco). O `CHANGELOG.md` copiado é o do
`stub`: esvazie-o e comece pela versão que o seu `meson.build` declara. Só uma correção é
necessária no `Makefile` copiado: `stub/` mora em `models/player/fixtures/stub/` (quatro níveis
até a raiz do `poc-mixr`, `ROOT := $(abspath ../../../..)`), enquanto `models/player/meu-modelo/`
mora só três níveis abaixo — troque a linha `ROOT := $(abspath ../../../..)` para
`ROOT := $(abspath ../../..)`, ou o Makefile vai apontar para o diretório **pai** do `poc-mixr` e
falhar em `check-root` com um caminho que não existe.
Depois disso, `cd models/player/meu-modelo && make && make test && make install-host` funciona
igual ao `stub` (ver [player/fixtures/stub/README.md](player/fixtures/stub/README.md) para o que cada alvo faz).

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
   **Nunca** as libs de `libs/` que são estáticas (`xtacview`, `xclock`, `xjoystick`, `xmsg`) —
   você ganharia uma cópia privada dos estáticos delas.

**Nada a acrescentar ao `models:` do [Makefile](../Makefile).** Desde que ele migrou para
descoberta automática por `find` (`MODELOS_PRODUCAO`, ver §1), todo projeto sob `models/player/`
(fora de `template/`) já entra sozinho em `make models`/`make test` — confirme rodando `make
models` na raiz e conferindo que o nome do seu modelo aparece no log.

### 2.2 O que o `.cpp` tem de ter

Leia **[player/fixtures/stub/docs/CONTRATO.md](player/fixtures/stub/docs/CONTRATO.md)** — é a lista completa. O resumo:

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

### 2.4 A alternativa em camadas: `player/template/`

O `stub` (§3) é deliberadamente achatado — um arquivo, sem `domain/`, sem separação nenhuma —
porque o papel dele é provar que o contrato de plugin **basta**, não ensinar arquitetura.
[`player/template/`](player/template/README.md) existe para o caso oposto: você já sabe que vai
escrever mais de uma decisão coordenada e quer começar já na forma que `A4`/`missile` usam, só que
reduzida ao mínimo que ainda compila e roda — `domain/` (uma regra pura, um Schmitt trigger),
`ubf/` (percepção → decisão → ação, incluindo a escrita obrigatória no `xboard`) e `xnative/`
(o registro que a fronteira do plugin chama).

A cópia funciona igual à do `stub`, só que com mais arquivos para renomear (o namespace C++
incluído — `player/template/docs/PRIMEIROS-PASSOS.md` tem o roteiro completo, com os comandos
exatos):

```bash
cp -r models/player/template models/player/meu-modelo
```

`player/template/docs/ARCHITECTURE.md` explica o porquê de cada camada, o porquê de `domain::`
morar aninhado no namespace do próprio modelo (evita colisão de `type_info` quando dois plugins
carregam juntos no mesmo processo — mesmo raciocínio do `missile`), e o roteiro para crescer até
uma árvore do BehaviorTree.CPP quando uma regra só deixar de bastar. Como o `stub`, `template`
**nunca** entra no alvo `models:` do Makefile raiz nem em `tests/meson.build` — não é produção,
nenhum cenário aponta para ele.

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

**A poc não tem código.** Isto mudou desde que `./app` virou o runner único de todo o
repositório — não existe mais um `main.cpp`/`mixr_factory.cpp` por poc (essa camada foi dissolvida
por construção; ver `CLAUDE.md`, "Estrutura de um subprojeto"). Criar uma poc nova é, hoje, três
passos de **dado**, não de código:

1. **`src/poc/<nome>/`**: `configs/scenario.edl.in` (o cenário) + `data/` (com os `.gitkeep`
   necessários — `recordings/`, `logs/`, `messages/`, todos gitignorados) + `README.md` (o que
   esta poc isola). Nenhum `.cpp`/`.hpp`, nenhum `subdir()` em Meson — `src/poc/meson.build` está
   deliberadamente vazio desde essa mudança.
2. **No `.edl.in`**, o bloco de plugin como **primeira entrada de `components:`** — isto **não**
   mudou, é a mesma mecânica de sempre:

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
   > `isValid()` do `( PluginLoader )`. Fora de ordem, o `mixrFactory` aborta explicando isso —
   > não há silêncio nem SIGSEGV.

   > **`provides:` é igualdade EXATA de conjunto.** Se a `.so` não entregar exatamente esses
   > nomes, o processo morre dizendo o que ela entrega. É o que pega uma `.so` velha esquecida no
   > caminho. `mixr::xplugin::factory`/`mixr::xplugin::loadedFactory` já estão encadeadas no
   > `mixr_factory.cpp` único do host (`app/src/mixr_factory.cpp`) — nenhuma poc precisa da
   > própria cadeia de factory.
3. **Nada a registrar para ficar rodável** — não há mais catálogo estático de cenários no `./app`.
   Assim que `configs/` tiver um único `.edl`/`.edl.in`, a poc já é alcançável por
   `-folder src/poc -scenario <nome>` (ou `-folder src/poc/dis -scenario <nome>` se entrar no
   grupo DIS) — ver §4.1. Cobertura de teste automática é o único passo opcional, em §4.2.

`src/poc/dis/bandit/` é o exemplo mais enxuto de poc — nunca teve modelo local nenhum (o intruso
chega por DIS de fora), e ainda assim segue exatamente estes três passos.

### 4.1 Como a poc fica alcançável

Nenhum código/registro é necessário — `./app` não tem mais catálogo estático embutido no binário
(`app::ScenarioEntry`/`app::adHocScenario()`, em `app/src/app/AdHocScenario.cpp`, cobrem hoje só a
entrada de `-f <arquivo>`, com frota sempre `falcon1..4`). Duas formas de rodar a poc nova, ambas
sem tocar em C++:

- `./build/app/src/app -folder src/poc -scenario <nome>` (ou `-folder src/poc/dis -scenario <nome>`
  se a poc entrar no grupo DIS) — pula a tela de navegação. A frota é descoberta em runtime
  (`app::discoverFleet()`), então funciona com qualquer nome de player, não só `falcon1..4`.
- `./build/app/src/app -f src/poc/<nome>/configs/scenario.edl.in` — direto pelo caminho; assume a
  frota `falcon1..4` (não serve para uma poc com frota diferente, como `full-systems-nav`).

Único requisito estrutural: `configs/` precisa ter **exatamente um** `.edl`/`.edl.in` para
`-folder` conseguir descobrir a poc sem ambiguidade (ver `app/ScenarioFolder.hpp`).

### 4.2 Cobertura de teste automática (opcional)

A árvore de decisão sobre **onde** a poc ganha cobertura em `tests/meson.build` (marcadores no
próprio arquivo apontam de volta para cá):

| a poc... | cobertura | precedente |
|---|---|---|
| segue o formato dual `intruder`/`lowfuel` com rótulos `EVADE`/`SUPPORT`/`RTB` (semântica de interceptação aérea) | entra na lista `pocs` — ganha `scenario-*`/`memory-*`/`determinism-*` de graça, via `foreach` | `single-thread`, `multi-thread`, `python-flight` |
| não segue esse formato, mas tem uma propriedade que vale a pena provar (ex.: "decidiu em todos os frames", "dump byte-idêntico em 1/2/4 threads") | bloco(s) `test()` manuais, reaproveitando `scenario_runner`/`leak_runner`/`determinism_sh` (as mesmas ferramentas, fora do `foreach`) | `onnx-policy` (linhas 493-503 de `tests/meson.build`) |
| não precisa de nenhuma das duas — o cenário é só mais uma composição de players já testados em outro lugar | nenhuma entrada em `tests/meson.build`; documente o porquê no `README.md` da própria poc; determinismo continua verificável rodando `check_determinism.sh <binario> <rotulo> [frames] [poc] [arquivo-de-cenario]` direto (sem alvo de Makefile próprio) | `built-in_mixr_1`, `full-systems-nav` (zero linhas em `tests/meson.build`) |

Nenhuma das duas opções é obrigatória — a poc nova já roda (§4.1) sem cobertura de teste nenhuma.

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
   universal.** `models/player/A4/` e `models/player/missile/` ficam três níveis abaixo da
   raiz (`ROOT := $(abspath ../../..)`); `models/player/fixtures/stub/` fica quatro
   (`../../../..`). Copiar o `Makefile` do `stub` para um modelo novo direto sob `models/player/`
   (o caminho recomendado em §2) e esquecer de tirar um `../` faz `check-root` apontar para um
   diretório que não existe e falhar dizendo que o SDK não existe — mesmo com ele publicado.

---

## Ler também

- **[player/fixtures/stub/docs/CONTRATO.md](player/fixtures/stub/docs/CONTRATO.md)** — o que um modelo TEM de fazer
- **[player/fixtures/stub/README.md](player/fixtures/stub/README.md)** — o build autocontido do fixture,
  alvo por alvo (`make`, `make test`, `make install-host`)
- **[player/template/README.md](player/template/README.md)** — o build autocontido do template (mesmo
  molde do stub)
- **[player/template/docs/ARCHITECTURE.md](player/template/docs/ARCHITECTURE.md)** — as quatro camadas,
  o porquê de `domain::` morar aninhado no namespace do modelo, e quando crescer para uma árvore do
  BehaviorTree.CPP
- **[player/template/docs/PRIMEIROS-PASSOS.md](player/template/docs/PRIMEIROS-PASSOS.md)** — o roteiro
  mecânico de copiar o template e transformá-lo num modelo com nome próprio
- **[player/A4/docs/ARCHITECTURE.md](player/A4/docs/ARCHITECTURE.md)** — calibração e armadilhas do
  modelo de produção
- **[player/missile/docs/DESIGN.md](player/missile/docs/DESIGN.md)** — a lei de guiagem da demo de míssil
- **[../libs/xplugin/README.md](../libs/xplugin/README.md)** — o contrato de ABI e a seção
  **Limites**, que diz o que ele **não** garante
- **[../tests/README.md](../tests/README.md)** — as duas suítes e o que cada camada prova
- **[../src/poc/dis/single-thread/README.md](../src/poc/dis/single-thread/README.md)** — a dissecação profunda do
  modelo de produção (os arquivos moraram para cá, o texto continua valendo)
