---
paths:
  - "models/**"
---

# Modelo é plugin — regras de `models/`

- Cada `models/<players|systems|others>/<nome>/` é um projeto Meson **autocontido**: tem
  `Makefile`, `meson.build`, `tests/`, `docs/`, `README.md`, `CHANGELOG.md` próprios (guarda:
  `tests/guard/check_modelo_estrutura.sh`, que descobre por `find` sob QUALQUER subpasta de
  `models/`, não só `players/`). `cd models/<categoria>/<nome> && make` configura/builda/instala
  sozinho — só precisa do SDK publicado uma vez pela raiz (`make configure && make sdk`).
- **Nunca escreva direto em `dist/` a partir de um modelo.** `make install-host` de cada modelo
  deposita em `./plugins/` (raiz do repo, flat, não versionado); é `make install` (alvo
  `sync-plugins`) na raiz quem sincroniza `plugins/` → `dist/`. `dist/` e `build/` são gerados —
  nunca edite ou crie arquivo neles à mão.
- **Nomes de fábrica MIXR são globais ao processo, não por-plugin.** Dois `.so` carregados juntos
  publicando o mesmo nome derrubam o processo (`die()` em `PluginRegistry::loadModule`). Já
  aconteceu de verdade (`ThreadTagProbe`, A-4×missile — o extinto modelo de demo — ver CLAUDE.md,
  "vigésima terceira passada"). Depois de acrescentar uma classe nova a qualquer `factory.cpp`,
  rode `python3 tests/guard/check_colisao_fabrica.py` — o hook `check-colisao-fabrica.sh` já faz
  isso automaticamente após editar `.cpp`/`.hpp` sob `models/`.
- `provides:` no `.edl` é **igualdade exata de conjunto** contra o que o `.so` exporta. Acrescentar
  um nome novo de fábrica obriga atualizar `provides:` em **todo** cenário que carrega essa `.so`
  (produção e testes) — não só o cenário que motivou a mudança.
- Modelo novo: `make new-model NAME=<nome> CATEGORY=player|system|others` (`CATEGORY` é
  obrigatório e decide a subpasta — `player`→`models/players/`, `system`→`models/systems/`,
  `others`→`models/others/`; não existe `CATEGORY=event`, ver `scripts/models.sh` para o porquê).
  Não escreve lógica nenhuma, só copia o esqueleto de `models/template/`. Depois, siga
  `CONTRIBUTING.md` §5 para o cenário (não há catálogo para registrar — basta um `.edl.in` em
  `configs/`) e anote em `models/REGISTRO.md` (coordenação humana, sem enforcement automático).
- `models/template/` **não é produção** — mora no primeiro nível de `models/`, fora de qualquer
  categoria, justamente por isso, e não entra na checagem de colisão de fábrica.
  Hospeda DOIS artefatos: o scaffold copiável (`libtemplate.so`, exemplo em camadas) e um mirror
  de contrato (`libtemplate_mirror.so`, `src/mirror.cpp` — **não** faz parte do scaffold, apagar ao
  copiar) que exporta de propósito os mesmos nomes/slots do cenário de produção, para
  `plugin-modelo-estranho`/`plugin-deposito-terceiro` provarem que o contrato basta contra um
  modelo desconhecido. Herdou esse papel de `models/players/fixtures/stub/` (removido).

Ver CLAUDE.md, seção "O MODELO é um plugin, construído numa etapa PRÉVIA", para o detalhe completo.
