# `models/REGISTRO.md` — quem já existe, quem cuida

Coordenação entre devs, leitura humana — não é usado por nenhum alvo de build/teste/guard (a
descoberta de modelos continua via `find`). Existe para responder, num relance, "quem já está
mexendo em qual modelo".

Atualização é manual, sem enforcement automatizado. Ao abrir um PR de modelo novo, acrescente sua
própria linha no **fim** da tabela; ao mudar algo, edite só a sua linha, nunca a de outro modelo.

| Modelo | Pasta | Responsável | Última atualização | Observação |
|---|---|---|---|---|
| A-4 (flight) | `models/players/A-4/` | — | — | produção — `single-thread`, `multi-thread`, `bandit`, `python-flight`, `onnx-policy`, `built-in_mixr_1` |
| missile | `models/players/missile/` | — | — | produção, demo acadêmica — `scenario_missile_demo.edl.in` (via `single-thread`) |
| fixtures/stub | `models/players/fixtures/stub/` | — | — | fixture de teste, não é modelo de produção — não reivindicar |
| template | `models/players/template/` | — | — | ponto de partida, não é modelo de produção — não reivindicar |
| helicóptero | `models/players/<a definir>/` | a definir | — | planejado |
| disco-voador | `models/players/<a definir>/` | a definir | — | planejado |
| satélite | `models/players/<a definir>/` | a definir | — | planejado |
| foguete | `models/players/<a definir>/` | a definir | — | planejado |

Quem começar um dos quatro planejados: troque `models/players/<a definir>/` pela pasta real e
`a definir` pelo seu nome, na própria linha — sinal para os outros de que já tem dono.

## Ler também

- [`../CONTRIBUTING.md`](../CONTRIBUTING.md) — o roteiro completo de contribuir com um modelo novo
- [`README.md`](README.md) — visão geral de `models/`, o build em etapas, como registrar um
  cenário novo (seções 4.1 e 4.2)
