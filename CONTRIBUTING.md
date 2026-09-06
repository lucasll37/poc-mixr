# Contribuindo com um modelo novo

Este arquivo não repete o que já está escrito em outro lugar — ele COSTURA, na ordem certa, os
documentos que já são autoridade sobre cada assunto. Cada passo abaixo aponta para o documento
certo; leia-o antes de seguir para o próximo passo. Se você está mudando o *host*
(`app/`, `src/`, `shared/`) em vez de escrever um modelo novo, pule para a seção final.

## 0. O que você vai construir

Um **modelo** é uma biblioteca (`.so`) compilada à parte, aberta em tempo de execução via
`dlopen` — o host nunca vê seu código-fonte. Se este parágrafo é novidade, leia primeiro
[`README.md`](README.md), seção 8 ("Os modelos são plugins"), antes de continuar.

Antes de começar, confira [`models/REGISTRO.md`](models/REGISTRO.md) — a tabela de quem já está
mexendo em qual modelo. Se o modelo que você quer escrever já tem alguém trabalhando nele, evite
duplicar; se não, essa é a hora de acrescentar sua própria linha.

Para entender o **framework** por baixo dos modelos (MIXR/BehaviorTree.CPP) além do necessário
para este roteiro, `contexts/mixr-report.pdf` e `contexts/bt-report.pdf` são os manuais técnicos
completos (13 capítulos, leitura contínua) — os `.md` em `contexts/*-CONTEXT.md` são destilações
voltadas a consulta rápida/IA, não substituem os PDFs para quem está aprendendo do zero.

## 1. Pré-requisitos e build do host (uma vez por máquina)

→ [`README.md`](README.md), seções 2 e 3.

```bash
make configure && make sdk
```

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

→ [`tests/README.md`](tests/README.md) — as camadas e o que cada uma prova.

```bash
meson configure build -Dtests=true && make build
make test          # host + modelo(s)
make test-models    # só o(s) modelo(s)
```

Ou, de dentro do seu próprio `models/player/<nome>/`, sem tocar no resto do repositório:

```bash
make test
```

## 7. Antes de abrir o PR

Checklist — não depende de nenhum arquivo do GitHub, é só uma lista de texto:

- [ ] **Branch e commit** seguem a convenção da seção abaixo.
- [ ] **Reveja o checklist do seu ponto de partida** (`PRIMEIROS-PASSOS.md`, seção final, se veio
      do `template`, ou o que `scripts/new_model.py` imprimiu ao final) — é sobre o seu código, e é
      diferente desta lista, que é sobre o que a revisão vai conferir.
- [ ] Se o seu modelo é novo, **acrescente sua linha em [`models/REGISTRO.md`](models/REGISTRO.md)**
      (nome, pasta, status → `em-revisão`, cenário(s), link do PR) — é como quem revisa (e os
      outros três modelos em paralelo, se houver) sabe o que está em andamento.
- [ ] `make test` local passou (host + modelo(s)).
- [ ] `provides:` do `.edl` bate EXATAMENTE com o que o `.so` exporta.
- [ ] `CHANGELOG.md` do modelo começou do zero, na versão do `meson.build`.
- [ ] Se o cenário deveria aparecer no `./app`, a `ScenarioEntry` foi registrada
      (`models/README.md` §4.1) — sem isso, ninguém consegue rodar o modelo pelo `./app`.
- [ ] Se você tocou em algum dos arquivos hotspot listados em "Pontos de conflito de merge
      conhecidos" abaixo, confirme que a edição foi no ponto de inserção marcado no próprio
      arquivo (comentário `>>> ... <<<`), não no meio de uma entrada existente.
- Rode as duas guardas que existem justamente para pegar o que a revisão humana não pegaria
  sozinha (fazem parte de `make test`, suíte `guard`):
  ```bash
  meson test -C build --suite guard
  ```
  Em especial `modelo-estrutura` (as cinco peças que todo `models/player/<nome>/` precisa ter) e
  `colisao-fabrica` (nenhum nome de fábrica seu colide com A4/missile — se colidiu, renomeie o
  lado que NÃO é produção; precedente: `ThreadTagProbe` → `MissileThreadTagProbe`, ver
  `CLAUDE.md`).

## Convenção de branch, commit e PR

**Branch**: `<tipo>/<escopo>/<slug-curto>`, onde `tipo` é um dos usados no histórico deste
repositório (`feat`, `fix`, `docs`, `test`, `refactor`, `chore`) e `escopo` é a pasta mais afetada
— o nome do modelo (`models/player/<nome>` → escopo `<nome>`) ou uma área já usada no
`app`/`src`/`shared` (`app`, `rl`, `xlog`, `poc`, `flight`, ...). Exemplos:
`feat/patrulha-costeira/scaffold-inicial`, `fix/A4/altitude-safety-histerese`.

**Commit e título do PR**: `tipo(escopo): descrição curta, português, minúscula, sem ponto
final` — não é uma convenção nova: é a que os commits `a6caebb`..`66866d1` deste repositório já
usaram (confira com `git log --oneline`) antes do hábito lapsar de volta para `up`. A regra do
`CHANGELOG.md` de cada modelo continua igual — **a data sai do commit, nunca da mensagem**
(`models/README.md`, seção sobre `CHANGELOG.md`) — o que muda é que a MENSAGEM agora carrega
significado que dá para colar quase literal na entrada do CHANGELOG, em vez de precisar
reconstruir a partir do diff.

**Merge**: squash-and-merge — cada PR vira UM commit em `main`, com o título do PR como
mensagem. Assim dá para commitar rascunho quantas vezes quiser dentro da própria branch sem sujar
o histórico principal; a única mensagem que precisa estar certa é o título do PR.

## Pontos de conflito de merge conhecidos

Se dois PRs de modelos diferentes estiverem abertos ao mesmo tempo, estes são os arquivos que
mais provavelmente colidem, em ordem de probabilidade:

- **[`app/src/app/ScenarioCatalog.cpp`](app/src/app/ScenarioCatalog.cpp)** — toda vez que um
  cenário novo precisa aparecer no catálogo do `./app` (`-scenario <chave>`), a `ScenarioEntry`
  entra aqui (`models/README.md` §4.1). O arquivo tem um comentário-marcador
  (`>>> PROXIMO MODELO/CENARIO AQUI <<<`) logo antes do fechamento da lista — acrescente seu bloco
  ali, sempre no fim, nunca no meio das entradas existentes. Se conflitar mesmo assim, a resolução
  é sempre "manter as duas adições", nunca escolher uma.
- **[`tests/meson.build`](tests/meson.build)** — a lista `pocs` (só para cenários no formato
  EVADE/SUPPORT/RTB) e o bloco de `test()` manuais (para os que não seguem esse formato) têm cada
  um seu próprio comentário-marcador no arquivo — ver `models/README.md` §4.2 para qual dos dois
  usar. Mesma regra: insira no marcador, no fim, nunca no meio.
- **`models/REGISTRO.md`** — cada modelo acrescenta a própria linha, no fim da tabela, nunca edita
  a linha de outro.
- **`Makefile` raiz** — hoje `models:`/`clean:` descobrem projetos por `find`
  (`MODELOS_PRODUCAO`, ver `models/README.md` §1); um modelo novo sob `models/player/<nome>/` não
  precisa de linha nenhuma aqui. Esta entrada existia como hotspot antes dessa migração — mantida
  só para registrar que já foi resolvida.
- `conanfile.py` — se o seu modelo precisar de uma dependência Conan nova (raro).
- `src/ui/edl_catalog.generated.json` — regenerado por `make edl-catalog`; se o seu modelo
  acrescenta classes/slots novos ao catálogo, regenere DEPOIS de resolver o merge, não tente
  resolver o diff do JSON à mão.

## Se você está mudando o host, não um modelo

Não há um documento de onboarding equivalente para `app/`/`src/`/`shared/` — o mais próximo é
[`CLAUDE.md`](CLAUDE.md), que é um log histórico e uma referência de arquitetura do painel, não um
roteiro. Para testes, `tests/README.md` vale igual. Use o mesmo checklist da seção 7 acima, lendo
"modelo" como "a parte do host que você mudou", e preste atenção especial aos hotspots listados em
"Pontos de conflito de merge conhecidos" — a maioria deles é código de host, não de modelo.

## Ler também

- [`models/README.md`](models/README.md) — visão geral de `models/`, o build em etapas, como
  registrar um cenário novo (seções 4.1/4.2)
- [`models/REGISTRO.md`](models/REGISTRO.md) — quem já existe, quem cuida
- [`models/player/fixtures/stub/docs/CONTRATO.md`](models/player/fixtures/stub/docs/CONTRATO.md) — o contrato normativo
- [`models/player/template/docs/ARCHITECTURE.md`](models/player/template/docs/ARCHITECTURE.md) — as camadas e o porquê
- [`tests/README.md`](tests/README.md) — as suítes de teste
- `contexts/mixr-report.pdf` / `contexts/bt-report.pdf` — os manuais técnicos completos de MIXR e
  BehaviorTree.CPP
