---
paths:
  - "models/**"
---

# Modelo é plugin — regras de `models/`

- Cada `models/players/<nome>/` é um projeto Meson **autocontido**: tem `Makefile`, `meson.build`,
  `tests/`, `docs/`, `README.md`, `CHANGELOG.md` próprios (guarda:
  `tests/guard/check_modelo_estrutura.sh`). `cd models/players/<nome> && make` configura/builda/
  instala sozinho — só precisa do SDK publicado uma vez pela raiz (`make configure && make sdk`).
- **Nunca escreva direto em `dist/` a partir de um modelo.** `make install-host` de cada modelo
  deposita em `./plugins/` (raiz do repo, flat, não versionado); é `make install` (alvo
  `sync-plugins`) na raiz quem sincroniza `plugins/` → `dist/`. `dist/` e `build/` são gerados —
  nunca edite ou crie arquivo neles à mão.
- **Nomes de fábrica MIXR são globais ao processo, não por-plugin.** Dois `.so` carregados juntos
  publicando o mesmo nome derrubam o processo (`die()` em `PluginRegistry::loadModule`). Já
  aconteceu de verdade (`ThreadTagProbe`, A-4×missile — ver CLAUDE.md, "vigésima terceira passada").
  Depois de acrescentar uma classe nova a qualquer `factory.cpp`, rode
  `python3 tests/guard/check_colisao_fabrica.py` — o hook `check-colisao-fabrica.sh` já faz isso
  automaticamente após editar `.cpp`/`.hpp` sob `models/`.
- `provides:` no `.edl` é **igualdade exata de conjunto** contra o que o `.so` exporta. Acrescentar
  um nome novo de fábrica obriga atualizar `provides:` em **todo** cenário que carrega essa `.so`
  (produção e testes) — não só o cenário que motivou a mudança.
- Modelo novo: `make new-model NAME=<nome> KIND=stub|template` (não escreve lógica nenhuma, só
  copia o esqueleto certo). Depois, siga `CONTRIBUTING.md` §5 para o cenário (não há catálogo
  para registrar — basta um `.edl.in` em `configs/`) e anote em `models/REGISTRO.md` (coordenação
  humana, sem enforcement automático).
- `models/players/fixtures/stub/` e `models/players/template/` **não são produção** — não entram na
  checagem de colisão de fábrica. Exceção: quando `plugin-modelo-estranho`/`plugin-deposito-terceiro`
  rodam o cenário de produção contra o stub, ele precisa continuar aceitando/publicando os mesmos
  slots/nomes que o cenário de produção usa (mesmo que só para ignorar o valor).

Ver CLAUDE.md, seção "O MODELO é um plugin, construído numa etapa PRÉVIA", para o detalhe completo.
