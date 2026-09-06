---
paths:
  - "app/**"
  - "src/**"
  - "shared/**"
  - "meson.build"
---

# Host opaco — regras de `app/`, `src/`, `shared/`

- **Invariante mais citado do projeto**: o build do HOST (`app/`, `src/`, `shared/`, `meson.build`
  raiz) nunca referencia, inclui ou compila fonte de MODELO (`domain/`, `bt/`, `ubf/`, `xnative/`
  — essas árvores moram só em `models/player/<nome>/`). O host só consome o `.so` publicado em
  `dist/lib/mixr-plugins/`, carregado por `dlopen` em runtime. Guarda:
  `tests/guard/check_host_opaco.sh` (o hook `check-host-opaco.sh` já roda isso automaticamente
  depois de editar algo sob estas pastas).
- **`shared/x<nome>/` só vira `shared_library()` quando cruza a fronteira de plugin `dlopen`** —
  hoje são seis: `xboard`, `xlog`, `xtrack`, `xrlbridge`, `xinfer`, `xpyembed`. As demais
  (`xtacview`, `xclock`, `xjoystick`, `xmsg`, `xplugin`, `xrandom`) ficam estáticas/header-only —
  um plugin que linkasse uma delas ganharia cópia PRÓPRIA do estado estático, quebrando o
  compartilhamento host↔plugin que é a razão de existir da lib. Não promova uma dessas para
  `shared_library()` sem entender por que ela ainda não precisou.
- **`./app` é o runner único das pocs.** `src/poc/<nome>/` não tem `.cpp`/`.hpp` — é só
  `configs/`+`data/`+`README.md`. Cenário novo: registrar em `app/src/app/ScenarioCatalog.cpp`,
  nunca criar executável próprio.
- Toda pasta `<poc>/data/{recordings,logs,messages}/` precisa existir em disco (com `.gitkeep`)
  antes da simulação rodar — `TacviewOutput`/`PrintHandler`/`MsgFileSink` falham em SILÊNCIO sem o
  diretório (não é exceção, é log mudo). Ao criar uma poc/cenário novo sob esse padrão, replique
  os `.gitkeep`.
- Comentário e mensagem de console em português; identificador/slot/nome de fábrica em inglês
  (convenção do próprio MIXR).

Ver CLAUDE.md, seções "Estrutura de um subprojeto" e "O modelo MIXR em uma tela", para mais.
