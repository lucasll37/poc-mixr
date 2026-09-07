# `docs/manual/` — explorador de execução, EDL e classes built-in do MIXR

Página estática (React embutido, zero rede para abrir) com duas visões sobre o framework,
**curadas, não instrumentadas** — sem processo MIXR rodando por trás, herança, nome de fábrica,
registro, slots, fases e os trechos de código (com arquivo e linha reais) vêm de
`tools/extract_execution_chain.py` escaneando o fonte de verdade (`contexts/src/mixr/`) — nada
digitado à mão.

## Como se usar

```bash
xdg-open docs/manual/index.html   # ou make open-docs -- zero rede, nenhum servidor
```

**Execução** — o ciclo de fases do MIXR (dynamics/transmit/receive/process + as duas threads de
decisão/fundo) desenhado sobre a árvore de um `( Aircraft )` com os dez sistemas primários que
`Player::updateSystemPointers()` resolve por tipo (~72 nós, o mesmo cenário exaustivo de
`src/poc/built-in_mixr_1`). Grafo navegável (pan/zoom/arrastar, timeline com transporte,
tema claro/escuro); clicar num nó fixa um popup com nome de fábrica/registro/contagem de slots;
clicar num quadradinho de fase pula direto pro passo em que aquele nó roda naquela fase.

**Catálogo** — as 342 classes que o fork declara, cruzadas com quem de fato se registra em
fábrica, quem tem slot e quem participa do despacho por fase. Busca por classe/fábrica/slot,
filtros (nome divergente, trabalha em fase, não registrada, no cenário, decisão/UBF); clicar
numa classe mostra o corpo real de qualquer método que ela sobrescreve, quando conhecido.

## Regenerar depois de editar `doc.jsx`

```bash
make docs   # ou: node docs/manual/compile.js
```

A primeira execução baixa React/ReactDOM UMD + Babel para `docs/manual/.cache/` (gitignored); as
próximas rodam sem rede. `index.html` é committed — abrir a página não exige gerar nada antes.

## Leia mais

[CLAUDE.md](../../CLAUDE.md), seção `docs/` — toda decisão de design e armadilha confirmada
rodando. O editor gráfico de cenário `.edl` (ferramenta de autoria, não visualização) mora em
[`src/ui/`](../../src/ui/README.md), não aqui.
