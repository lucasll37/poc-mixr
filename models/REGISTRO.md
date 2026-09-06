# `models/REGISTRO.md` — quem já existe, quem cuida

Esta tabela é **leitura humana e coordenação entre desenvolvedores**, nada mais. Ela **não** é
usada por nenhum alvo de build/teste/guard — a descoberta de modelos continua 100% via `find`
(`tests/guard/check_modelo_estrutura.sh`, `MODELOS_PRODUCAO` no `Makefile` raiz, `.gitignore`),
exatamente como já era antes deste arquivo existir. Ele existe só para responder, num relance,
"quem já está mexendo em qual modelo" — importante com vários modelos novos entrando em paralelo
(`patrulha costeira/helicóptero`, `disco-voador`, `satélite`, `foguete`, ver `TODO.md`).

**Atualização é manual, por convenção — sem enforcement automatizado**, o mesmo espírito do
`CHANGELOG.md` de cada modelo (que também não é verificado por script nenhum). Ao abrir um PR de
modelo novo, acrescente a própria linha; ao mudar de status, edite a sua. Nunca edite a linha de
um modelo que não é seu, e sempre acrescente linha nova no **fim** da tabela — dois PRs que só
*acrescentam* linha no fim colidem da forma mais barata de resolver (a fusão de duas adições,
nunca a escolha de uma só).

| Modelo | Pasta | Tipo | Status | Responsável | Cenário(s) | Última atualização |
|---|---|---|---|---|---|---|
| A4 (flight) | `models/player/A4/` | produção | produção | — | `single-thread`, `multi-thread`, `bandit`, `python-flight`, `onnx-policy`, `built-in_mixr_1`, `app` (patrol/intercept/intercept_missile) | — |
| missile | `models/player/missile/` | demo acadêmica | produção | — | `scenario_missile_demo.edl.in` (via `single-thread`) | — |
| fixtures/stub | `models/player/fixtures/stub/` | fixture de teste | não é modelo de produção — não reivindicar | — | nenhum (só testes de plugin) | — |
| template | `models/player/template/` | ponto de partida | não é modelo de produção — não reivindicar | — | nenhum | — |
| helicóptero | `models/player/<a definir>/` | produção | planejado | a definir | a definir | — |
| disco-voador | `models/player/<a definir>/` | produção | planejado | a definir | a definir | — |
| satélite | `models/player/<a definir>/` | produção | planejado | a definir | a definir | — |
| foguete | `models/player/<a definir>/` | produção | planejado | a definir | a definir | — |

**Vocabulário fechado de `Status`**: `planejado` (ninguém começou ainda) / `rascunho` (scaffold
criado, lógica em progresso) / `em-revisão` (PR aberto) / `produção` (mergeado, cenário(s)
apontando pra ele) / `pausado` (começou e parou — deixe uma nota em "Cenário(s)" ou no próprio PR
sobre onde parou, para quem retomar depois).

Quem começar um dos quatro modelos planejados: troque `models/player/<a definir>/` pela pasta
real, `a definir` pelo seu nome e `planejado` por `rascunho`, na sua própria linha — é o sinal para
os outros três de que aquele modelo já tem dono.

## Ler também

- [`../CONTRIBUTING.md`](../CONTRIBUTING.md) — o roteiro completo de contribuir com um modelo novo
- [`README.md`](README.md) — visão geral de `models/`, o build em etapas, como registrar um
  cenário novo (seções 4.1 e 4.2)
