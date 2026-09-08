# `models/REGISTRO.md` — quem já existe, quem cuida

Coordenação entre devs, leitura humana — não é usado por nenhum alvo de build/teste/guard (a
descoberta de modelos continua via `find`). Existe para responder, num relance, "quem já está
mexendo em qual modelo".

Atualização é manual, sem enforcement automatizado. Ao abrir um PR de modelo novo, acrescente sua
própria linha no **fim** da tabela; ao mudar algo, edite só a sua linha, nunca a de outro modelo.

| Modelo | Pasta | Responsável | Última atualização | Observação |
|---|---|---|---|---|
| A-4 (flight) | `models/players/A-4/` | — | — | produção — `flight`, `bandit`, `python-flight`, `onnx-policy`, `built-in_mixr_1` |
| template | `models/players/template/` | — | — | ponto de partida, não é modelo de produção — não reivindicar; também hospeda o mirror de contrato (`src/mirror.cpp`) usado pelos testes de plugin do host |


## Ler também

- [`../CONTRIBUTING.md`](../CONTRIBUTING.md) — o roteiro completo de contribuir com um modelo novo,
  inclusive registrar um cenário novo (seção 5) e decidir a cobertura de teste (seção 5.3)
- [`../CLAUDE.md`](../CLAUDE.md), seção "O MODELO é um plugin, construído numa etapa PRÉVIA" —
  visão geral de `models/` e o build em etapas do repositório inteiro
