# Contribuindo com um modelo novo

Roteiro para escrever um **modelo** (a lógica de decisão de um novo componente).

## 1. O que você vai construir

Um modelo é um `.so` compilado à parte, aberto em runtime via `dlopen` — o core nunca vê seu
fonte nem precisa dele para compilar. São dois projetos Meson **independentes**, cada um com o
próprio `Makefile`: o da raiz (orquestra o core e todos os modelos de uma vez) e o de cada projeto
de modelo (autocontido — `cd` até lá e `make` já configura/builda/testa/instala sozinho, sem
chamar o Makefile da raiz). Fatos mínimos:

- Decide via um agente **UBF** (mecanismo nativo do MIXR para plugar decisão externa) — quase
  sempre uma árvore BehaviorTree.CPP por trás.
- `provides:` no `.edl` tem que bater EXATAMENTE com o que o `.so` exporta — um nome a mais ou a
  menos aborta a inicialização (com mensagem clara).
- Escrever no `xboard` é obrigatório e **falha em silêncio** se esquecido — sem erro, só
  `bt=--`/`dec=0` para sempre.
- Ponto de partida copiável: `models/template/`. `make new-model` copia e renomeia por você — o
  comando completo está no Passo 3.

**Convenção deste roteiro**: a raiz do repositório só entra nos Passos 2 (publicar o SDK, uma vez)
e 3 (criar o scaffold). A partir do Passo 3 você `cd` para dentro do diretório do seu modelo e
**fica ali** — é o diretório ativo do resto do roteiro; os passos seguintes voltam para a raiz só
onde o texto disser explicitamente.

## 2. Pré-requisitos e SDK do core

**Na raiz do repositório**, uma vez: → [`README.md`](README.md) ("Pré-requisitos"/"Build"). Não
precisa compilar o core inteiro — basta chegar até a publicação do SDK:

```bash
make configure
make sdk
```

Isso publica em `dist/` o contrato de ABI (`libs/xplugin/PluginAbi.hpp`) e as libs compartilhadas
core↔modelo. É o que o alvo `check-root` de todo `Makefile` de modelo confere antes de deixar
configurar/buildar/testar — sem isso publicado, qualquer comando do Passo 3 em diante falha com
uma mensagem dizendo exatamente o que rodar.

## 3. Escolha o ponto de partida

Ainda na raiz:

```bash
make new-model NAME=meu-modelo CATEGORY=players/air
```

`CATEGORY` é obrigatório — o caminho, relativo a `models/`, onde o scaffold entra (`players/air`,
`players/ground`, `systems/trackmanager`, `others`, ...; não é um enum fixo). Não existe
`CATEGORY=events`/`template` (ver [`models/events/README.md`](models/events/README.md)).

O gerador copia e renomeia mecanicamente (projeto, namespace, `Makefile`), remove o que não faz
parte do scaffold (`mirror.cpp`), e termina com um checklist do que ainda falta escrever. Roteiro
manual completo → [`models/template/docs/PRIMEIROS-PASSOS.md`](models/template/docs/PRIMEIROS-PASSOS.md).

**Daqui em diante, `cd models/<CATEGORY>/<nome>/` e fique ali.** É o diretório do projeto Meson do
seu modelo, com `Makefile` **próprio e autocontido** — não chama o Makefile da raiz, só precisa do
SDK que o Passo 2 já publicou lá. Os comandos dos próximos passos assumem esse diretório como
ativo; onde algum precisar voltar para a raiz, o texto avisa.

## 4. O contrato

→ [`models/template/docs/CONTRATO.md`](models/template/docs/CONTRATO.md) — leia inteiro. Três
obrigações merecem destaque: `provides:` bate exatamente com o `.so`; escrever no `xboard` (falha
em silêncio se esquecido); namespace aninhado sob `mixr::models::x<nome>` (evita colisão de RTTI
entre plugins).

## 5. Escreva a lógica

Trabalho de domínio — sem receita mecânica. Siga o Passo 5 de
`docs/PRIMEIROS-PASSOS.md`: domain → ubf/State → bt/nodes + árvore (`configs/`) → ubf/Behavior →
ubf/Action → xnative/factory → `plugin.cpp`.

**Groot** (editor/monitor visual de árvores BT.CPP) ajuda a editar o `.xml` e monitorar ao vivo —
os três alvos abaixo são do `Makefile` do seu modelo, sem sair do diretório dele:

```bash
make open-groot   # abre a janela (instalação: INSTALL.md §4)
make create-bt    # gera uma arvore vazia com os nos do SEU modelo
make update-bt    # resincroniza o <TreeNodesModel> apos registrar um no novo
```

## 6. Publique e aponte um cenário

Ainda no diretório do seu modelo:

```bash
make install-core   # publica em ../../plugins/, o depósito compartilhado com terceiros
```

`install-core` nunca escreve em `dist/` — quem sincroniza é o Makefile da raiz:

```bash
cd "$(git rev-parse --show-toplevel)"   # volta para a raiz -- a mesma do Passo 2
make install                            # plugins/ -> dist/: só isso deixa um cenário enxergar o .so
```

Alternativa, se preferir reconstruir TODOS os modelos do repositório de uma vez (o que `make test`
da raiz já espera antes de rodar a suíte do core) — os dois na raiz:

```bash
make models && make install
```

Registrar num cenário é só **dado** — sem catálogo estático para editar, nem uma linha de C++ a
mais. A forma mínima, para o `meu-modelo` do Passo 3:

```
src/poc/meu-cenario/
├── configs/
│   └── scenario.edl.in      # o cenário -- ver abaixo
├── data/
│   ├── recordings/.gitkeep  # gravação Tacview -- TacviewOutput falha em silêncio sem a pasta
│   ├── logs/.gitkeep        # libs/xlog
│   └── messages/.gitkeep    # libs/xmsg, só se o cenário declarar um MsgFeed
└── README.md                # o que ESTE cenário demonstra
```

```cpp
// src/poc/meu-cenario/configs/scenario.edl.in
( Station

   components: {

      // 1) SEMPRE a PRIMEIRA entrada de 'components:' -- o parser do EDL
      //    resolve na ordem do texto, entao o modelo precisa estar
      //    carregado antes de qualquer classe dele ser citada mais abaixo.
      plugins: ( PluginLoader
         searchPaths: {
            "./dist/lib/mixr-plugins/"   // onde o Passo 6 (make install) publicou o .so
         }
         modules: {
            ( PluginModule
               file:     "libmeu-modelo.so"
               // os MESMOS nomes que 'xnative/factory.cpp' do seu modelo
               // registra -- nem um a mais, nem um a menos, ou o processo
               // aborta na inicializacao mostrando as duas listas.
               provides: { ExampleState ExampleBehavior ExampleAction }
            )
         }
      )

      // 2) um player usando as classes que acabaram de ser carregadas
      meuPlayer: ( Aircraft
         initXPos: ( NauticalMiles 0 )
         initYPos: ( NauticalMiles 0 )
         initAlt:  ( Meters 2000 )
         components: {
            // o agente UBF nativo do MIXR -- decide em background, fora do
            // frame de tempo critico. 'state:'/'behavior:' sao as duas
            // classes que o SEU modelo forneceu; 'ExampleAction' nao
            // aparece aqui -- e devolvida em runtime por genAction(), so
            // precisa estar em 'provides:' acima.
            agent: ( UbfAgent
               state:    ( ExampleState )
               behavior: ( ExampleBehavior )
            )
         }
      )
   }
)
```

**Comentário de `.edl`/`.edl.in` tem que ser ASCII puro** — um único caractere acentuado dentro de
um `//`, em qualquer lugar do arquivo, faz o parser (bison/flex) recusar o arquivo **inteiro** com
`"syntax error"`, sem dizer por quê. O bloco acima já segue essa regra; escreva os seus
comentários sem acento pelo mesmo motivo.

Isso já é alcançável por `-folder`/`-file`, sem registrar em lugar nenhum (ver `README.md`, seção
"Rodar", para as duas flags). Para só testar rápido, sem criar uma pasta nova, copie um cenário
existente para `sandbox/` e troque só o bloco `PluginModule`.

**Terreno**: se o cenário sai da área coberta pelos 5 tiles SRTM versionados, baixe com
`scripts/fetch_srtm.sh <tile>` (ou `--bbox`/`--sudeste`/`--brasil`) — sem o tile, a falha é
silenciosa (elevação vira `0.0` sem aviso). Detalhe →
[`shared/data/terrain/srtm/README.md`](shared/data/terrain/srtm/README.md). Na raiz, gere e abra
o SVG de cobertura para conferir se a área do seu cenário já está baixada:

```bash
make terrain-coverage
```

**Cobertura de teste automática é opcional** — ver a tabela de critérios em `tests/meson.build`.

## 7. Teste

→ [`README.md`](README.md) ("Testes") e [`tests/README.md`](tests/README.md).

No diretório do seu modelo:

```bash
make test                 # só a suíte deste modelo -- recompila se precisar
make check-organization   # linter OPCIONAL de organização interna -- nunca bloqueia build/test/install
```

`check-organization` não roda em CI e nunca bloqueia `build`/`test`/`install` — é um checklist à
parte, opcional, mas rápido: camadas sem MIXR vazando (`domain/`, `bt/`), namespace aninhado sob
`mixr::models::x<nome>`, as três listas de `xnative/factory.cpp` em sincronia, a obrigação de
escrever no `xboard`, a versão do `CHANGELOG.md` batendo com a do `project()` do `meson.build`,
empacotamento correto do plugin (`shared_module()`, símbolo escondido), dado próprio publicado via
`install_data()`/`install_subdir()`, entre outras. É o jeito mais rápido de achar um
`xtemplate`/`Example` esquecido do scaffold sem esperar um erro de build ou de carga em runtime —
vale rodar antes de commitar.

Na raiz:

```bash
make test-models   # a mesma suíte de 'make test' acima, para TODOS os modelos descobertos -- a sua incluída
make test          # só a suíte do CORE -- exige os modelos já publicados em dist/ (Passo 6)
make test-asan     # o A-4 sob AddressSanitizer/LeakSanitizer (build separado, lento)
```

`test-asan` é hoje hardcoded para o A-4 — não cobre seu modelo automaticamente; para replicar, veja
`meson_options.txt`/`meson.build` do A-4 como referência.

## 8. Onde consultar durante o trabalho

- **Aprender o framework do zero**: [`docs/books/mixr-report.pdf`](docs/books/mixr-report.pdf) /
  [`bt-report.pdf`](docs/books/bt-report.pdf) (leitura completa), ou os três
  `contexts/*-CONTEXT.md` (destilação, consulta pontual). [`docs/manual/`](docs/manual/)
  (`make open-docs`) complementa com visões interativas.
- **Consulta rápida sobre uma classe/API**: os `contexts/*-CONTEXT.md`; quando não bastar,
  `contexts/src/` (fonte vendorizado) ou os headers do Conan. O agente `mixr-vendor-lookup` já
  sabe essa ordem.
- **Automação do repositório**: `.claude/rules/*.md` (contexto automático por caminho editado),
  `.claude/hooks/*.sh` (checagens pós-edição: core opaco, colisão de fábrica, lint de EDL).
