---
paths:
  - "**/*.edl"
  - "**/*.edl.in"
  - "**/*.edl.frag"
---

# Arquivos EDL (`.edl`, `.edl.in`, `.edl.frag`)

- **Comentário em EDL tem que ser ASCII puro.** Um único caractere acentuado dentro de um `//` já
  quebrou o parser bison/flex inteiro com `"syntax error"` apontando a linha certa mas sem dizer o
  motivo real (armadilha confirmada, ver CLAUDE.md, seção `src/poc/dis/bandit`). Escreva
  "nao"/"esta"/"e" sem acento em qualquer comentário `.edl`/`.edl.in`.
- `provides:` do bloco `( PluginModule )` é igualdade EXATA de conjunto contra o que o `.so`
  exporta — nome de fábrica faltando ou sobrando quebra a carga.
- As factories nativas `mixr::terrain::factory`, `mixr::dis::factory` e `mixr::linkage::factory`
  **não** são auto-encadeadas pelas demais (`simulation`/`models`/`recorder`) — sem
  `mixr_factory.cpp` encadeando a que falta, o bloco correspondente do `.edl`
  (`( SrtmHgtFile )`, `( DisNetIO )`, `( UsbJoystick )`) não constrói nada, **em silêncio**.
- `falcon1..4` dos cenários de produção compartilham o MESMO esqueleto de slots (guarda:
  `tests/guard/check_falcons_estrutura.sh`) — uma mudança de slot em um precisa entrar nos outros
  três também, a menos que seja um cenário deliberadamente assimétrico (ex:
  `src/poc/built-in_mixr_1/configs/scenario_max_player.edl.in`, que por isso não se chama
  `scenario.edl.in`, fora do glob que a guarda varre).
- Antes de considerar uma edição de `.edl`/`.edl.in` terminada: `make edl-lint FILE=<arquivo>`
  (lint leve; o hook `check-edl-lint.sh` já roda isso automaticamente) e, quando possível,
  `make edl-check FILE=<arquivo>` (o parser C++ de verdade — a autoridade final).

Ver CLAUDE.md, seção "Ao adicionar um subprojeto novo" e a armadilha 5 de `src/poc/dis/bandit`,
para o detalhe completo do parser.
