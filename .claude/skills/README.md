# Skills — nenhuma hoje

Nenhuma skill foi criada na reestruturação da camada de extensão (sessão de 2026-09-06): todo
procedimento repetível deste repositório já é um alvo de `Makefile` de uma linha
(`build`/`test`/`edl-lint`/`edl-check`/`docs`/...) — a premissa declarada do projeto é que "o
Makefile é a única fonte de automação" (não há CI configurado). O único fluxo multi-passo
genuinamente humano ("criar um modelo novo") já está coberto por `CONTRIBUTING.md` →
`models/README.md` → `models/REGISTRO.md`; duplicar isso numa skill não agregaria.

## Quando criar uma skill aqui

Crie `.claude/skills/<nome>/SKILL.md` quando aparecer algo que:

- **não** se resume a um único `make <alvo>` — ex.: uma sequência de decisões condicionais que
  hoje exigiria reexplicar toda vez ("qual dos dois pontos de partida copiar, `stub` ou
  `template`, dependendo de X"; ou um roteiro de diagnóstico de várias etapas para uma classe de
  bug recorrente); OU
- é material de referência longo demais para caber em `CLAUDE.md`/`.claude/rules/*.md` sem poluir
  o contexto de toda sessão, e só precisa ser consultado ocasionalmente — candidato natural: uma
  eventual divisão do `CLAUDE.md` (hoje ~292KB) em referências por subsistema, decisão que ficou
  em aberto na sessão de 2026-09-06 (o arquivo foi mantido intocado de propósito).

Frontmatter obrigatório: `name` + `description`. A `description` precisa dizer o que a skill faz
**e** quando usá-la, com frases-gatilho concretas — é o único texto dela que fica em contexto
permanentemente. Corpo com menos de 500 linhas; material longo vai para `references/` e é citado
explicitamente pelo `SKILL.md`.

Apague este arquivo assim que a primeira skill de verdade for criada — uma nota de "nada aqui
ainda" ao lado de skills reais só envelhece.
