# Contribuindo com um modelo novo

Este arquivo não repete o que já está escrito em outro lugar — ele COSTURA, na ordem certa, os
documentos que já são autoridade sobre cada assunto. Cada passo abaixo aponta para o documento
certo; leia-o antes de seguir para o próximo passo. Este roteiro é sobre escrever um **modelo**
novo — para mudar o *host* (`app/`, `src/`, `shared/`) não há roteiro equivalente; o mais próximo
é [`CLAUDE.md`](CLAUDE.md), que é referência de arquitetura, não passo a passo.

## 0. O que você vai construir

Um **modelo** é uma biblioteca (`.so`) compilada à parte, aberta em tempo de execução via
`dlopen` — o host nunca vê seu código-fonte. Se este parágrafo é novidade, leia primeiro
[`models/README.md`](models/README.md) antes de continuar.

Antes de começar, confira [`models/REGISTRO.md`](models/REGISTRO.md) — a tabela de quem já está
mexendo em qual modelo. Se o modelo que você quer escrever já tem alguém trabalhando nele, evite
duplicar; se não, essa é a hora de acrescentar sua própria linha.

Para entender o **framework** por baixo dos modelos (MIXR/BehaviorTree.CPP) além do necessário
para este roteiro → seção 7, "Onde consultar durante o trabalho".

## 1. Pré-requisitos e SDK do host (uma vez por máquina)

→ [`README.md`](README.md), seções "Pré-requisitos" e "Build" ([`INSTALL.md`](INSTALL.md) tem o
passo a passo comentado, se faltar algo). Para escrever um modelo, pare na etapa `make sdk` — não
precisa compilar o host inteiro (`make build`/`make install`) ainda.

Sem isso, todo `Makefile` autocontido de modelo (o seu vai ter um) falha em `check-root` dizendo
exatamente isto.

## 2. Escolha o ponto de partida

→ [`models/README.md`](models/README.md), seção 2, com a receita completa. Resumo:

| se o seu modelo... | comece por | por quê |
|---|---|---|
| decide com uma regra/condição só | [`models/player/fixtures/stub`](models/player/fixtures/stub/) | ~300 linhas, um arquivo, prova que o contrato basta |
| vai coordenar mais de uma decisão desde o início | [`models/player/template`](models/player/template/) | já nasce em camadas (`domain/`→`ubf/`→`xnative/`) |

**O caminho recomendado é o gerador automático**, que já existe neste repositório:

```bash
make new-model NAME=meu_modelo KIND=stub   # ou KIND=template
```

Ele faz a cópia e a renomeação mecânica por você (projeto, módulo, namespace, `ROOT` do Makefile
pela profundidade real) e termina com um checklist do que sobra manual. Se preferir fazer à mão, o
roteiro completo está em [`stub`, receita em `models/README.md` §2](models/README.md#2-como-criar-um-modelo-novo)
ou [`template`, `docs/PRIMEIROS-PASSOS.md`](models/player/template/docs/PRIMEIROS-PASSOS.md) passo
a passo, dos dois — mas o gerador cobre exatamente essa receita.

## 3. O contrato: o que TODO modelo tem que fazer

→ [`models/player/fixtures/stub/docs/CONTRATO.md`](models/player/fixtures/stub/docs/CONTRATO.md)
— leia inteiro, mesmo vindo do `template`. Três obrigações merecem destaque:

- **`provides:` bate EXATAMENTE com o que o `.so` exporta** (seção 2) — se não bater, o processo
  aborta na inicialização dizendo o que entregou; não é silencioso, mas é a causa mais comum de
  "meu cenário não sobe".
- **escrever no `xboard`** (seção 3) — a única obrigação que falha em **silêncio**: sem isso, o
  host sobe, roda, e a tela de status mostra `bt=--`/`dec=0` para sempre, sem erro em lugar
  nenhum.
- **namespace aninhado sob `mixr::models::x<nome>`** (seção 6) — sem isso, dois `.so` carregados
  juntos no mesmo processo podem colidir em RTTI silenciosamente.

## 4. Escreva a lógica

Depois do contrato, é trabalho de domínio — não tem receita mecânica. Se veio do `template`,
`docs/PRIMEIROS-PASSOS.md`, passo 5, sugere a ordem (domain → ubf/State → ubf/Behavior →
ubf/Action → xnative/factory).

## 5. Publique e aponte um cenário

→ `models/README.md`, seção 2.3 (verificar o `.so`) e seção 4 (registrar num `.edl`), ou
`PRIMEIROS-PASSOS.md`, passo 6, se veio do `template`.

O `.so` compilando e um `.edl` apontando pra ele **não** bastam para rodar via `./app -scenario
<chave>` nem para ganhar cobertura de teste automática — isso é um passo a mais, documentado em
`models/README.md`, seções **4.1** (registrar no catálogo do `./app`, em
`app/src/app/ScenarioCatalog.cpp`) e **4.2** (decidir se/como o cenário ganha teste automático em
`tests/meson.build`). Sem a 4.1, seu modelo compila e passa nos testes de plugin, mas ninguém
consegue rodá-lo pelo `./app`.

## 6. Teste

→ [`README.md`](README.md), seção "Testes", para o `-Dtests=true`/`make test` gerais, e
[`tests/README.md`](tests/README.md) para as camadas e o que cada uma prova.

Específico de modelo: `make test-models` roda só a suíte do(s) modelo(s) (pula a do host); de
dentro do seu próprio `models/player/<nome>/`, `make test` roda sozinho, sem tocar no resto do
repositório.

## 7. Onde consultar durante o trabalho

Três camadas diferentes, para perguntas diferentes:

**Aprender o framework do zero** — `contexts/mixr-report.pdf`/`contexts/bt-report.pdf` são os
manuais técnicos completos (13 capítulos cada) do MIXR e do BehaviorTree.CPP, leitura contínua.
[`docs/manual/`](docs/manual/) (`make open-docs`, página estática) complementa com três visões
interativas: a animação do ciclo de execução do MIXR sobre uma árvore de componentes real, um
catálogo buscável das classes do fork (fábrica, slots, participação por fase) e um ensaio sobre a
cadeia de decisão `FlightAgentTC → UBF → BehaviorTree`.

**Consulta rápida sobre uma classe/API específica (inclusive por um agente de IA)** — os três
`contexts/*-CONTEXT.md` (`MIXR-CONTEXT.md`, `MIXR-PATTERN-CONTEXT.md`, `BTCPP-CONTEXT.md`) são
destilações voltadas a resposta rápida, não substituem os PDFs acima para quem está aprendendo do
zero. Quando a destilação não basta ou parece contraditória, a autoridade final é o fonte
vendorizado em `contexts/src/` (git-ignored — não vem num clone limpo) ou os headers instalados
pelo Conan. O agente `mixr-vendor-lookup` (`.claude/agents/`) já sabe essa ordem de consulta e
devolve só o resumo, sem despejar C++ de terceiro na conversa.

**`.claude/` — automação e contexto para quem trabalha com um agente Claude Code neste repo**:

- `.claude/rules/*.md` carregam contexto extra sozinhas quando você edita um caminho que casa com
  o glob delas (`app/`+`src/`+`shared/`, `models/`, arquivos `.edl`/`.edl.in`/`.edl.frag`) — não
  precisam ser lidas à mão, mas valem uma olhada se quiser entender por que um hook bloqueou algo.
- `.claude/hooks/*.sh` rodam automaticamente depois de editar os arquivos correspondentes: host
  opaco (`check-host-opaco.sh`), colisão de nome de fábrica (`check-colisao-fabrica.sh`) e lint de
  EDL (`check-edl-lint.sh`) — a mesma checagem que `make test`/`make edl-lint` fariam, só que sem
  esperar o próximo build.
- `.claude/skills/README.md` e `.claude/mcp/README.md` documentam por que não há nenhum dos dois
  hoje, e quando criar um.
