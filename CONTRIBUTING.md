# Contribuindo com um modelo novo

Este arquivo não repete o que já está escrito em outro lugar — ele COSTURA, na ordem certa, os
documentos que já são autoridade sobre cada assunto. Cada passo abaixo aponta para o documento
certo; leia-o antes de seguir para o próximo passo. Se você está mudando o *host*
(`app/`, `src/`, `shared/`) em vez de escrever um modelo novo, pule para a seção final.

## 0. O que você vai construir

Um **modelo** é uma biblioteca (`.so`) compilada à parte, aberta em tempo de execução via
`dlopen` — o host nunca vê seu código-fonte. Se este parágrafo é novidade, leia primeiro
[`README.md`](README.md), seção 8 ("Os modelos são plugins"), antes de continuar.

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

Se existir na sua checkout um gerador automático (`make new-model NAME=... KIND=stub|template`
— ver `models/README.md`, seção 2, para confirmar se já foi implementado), ele faz a cópia e a
renomeação mecânica por você; senão, siga o roteiro manual do próprio ponto de partida:
[`stub`, receita em `models/README.md` §2](models/README.md#2-como-criar-um-modelo-novo) ou
[`template`, `docs/PRIMEIROS-PASSOS.md`](models/player/template/docs/PRIMEIROS-PASSOS.md) passo a
passo, dos dois.

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

- **Branch e commit** seguem a convenção da seção abaixo.
- **Reveja o checklist do seu ponto de partida** (`PRIMEIROS-PASSOS.md`, seção final, se veio do
  `template`) — ele é sobre o seu código; o checklist do PR (`.github/PULL_REQUEST_TEMPLATE.md`)
  é sobre o que a revisão vai conferir, e os dois não são o mesmo.
- Se o seu modelo é novo, **adicione uma linha em [`.github/CODEOWNERS`](.github/CODEOWNERS)**
  para `models/player/<seu-nome>/` — sem isso ninguém é notificado automaticamente quando alguém
  mexer nele depois.
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

Se dois PRs de modelos diferentes estiverem abertos ao mesmo tempo, estes arquivos são os que
mais provavelmente colidem — não por acaso: `.github/CODEOWNERS` já pede revisão extra sobre
eles.

- `Makefile` raiz (alvos `models:`/`clean:`, se o seu repositório ainda não migrou para
  descoberta automática por `find` — confira `models/README.md`, seção 1, para saber qual dos
  dois modos está em vigor).
- `conanfile.py` — se o seu modelo precisar de uma dependência Conan nova.
- `src/ui/edl_catalog.generated.json` — regenerado por `make edl-catalog`; se o seu modelo
  acrescenta classes/slots novos ao catálogo, regenere DEPOIS de resolver o merge, não tente
  resolver o diff do JSON à mão.

## Se você está mudando o host, não um modelo

Não há um documento de onboarding equivalente para `app/`/`src/`/`shared/` — o mais próximo é
[`CLAUDE.md`](CLAUDE.md), que é um log histórico e uma referência de arquitetura do painel, não um
roteiro. Para testes, `tests/README.md` vale igual. O checklist de PR de host está em
`.github/PULL_REQUEST_TEMPLATE.md`, seção "mudança no host".

## Ler também

- [`models/README.md`](models/README.md) — visão geral de `models/`, o build em etapas
- [`models/player/fixtures/stub/docs/CONTRATO.md`](models/player/fixtures/stub/docs/CONTRATO.md) — o contrato normativo
- [`models/player/template/docs/ARCHITECTURE.md`](models/player/template/docs/ARCHITECTURE.md) — as camadas e o porquê
- [`tests/README.md`](tests/README.md) — as suítes de teste
- [`.github/CODEOWNERS`](.github/CODEOWNERS) — quem revisa o quê
