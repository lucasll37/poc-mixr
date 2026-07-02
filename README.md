# mixr-hello

O exemplo mais simples possível usando o framework MIXR, seguindo o mesmo
padrão de build do projeto `my_mixr`: `Makefile` orquestrando `conanfile.py`
(Conan) e `meson.build` (Meson).

Cria uma `Station` com uma `Simulation` vazia (sem players, sem física,
sem `models`) e roda o ciclo `updateTC()`/`updateData()` por 5 segundos.

---

## Pré-requisitos

- O pacote Conan `mixr/1.0.5` já publicado no seu remote local
  (o mesmo gerado pelo `conanfile.py` do projeto `my_mixr` via
  `conan create .` ou `scripts/deploy.sh`)
- Conan ≥ 2.0
- Meson ≥ 1.0 e Ninja
- GCC ≥ 7 ou Clang ≥ 5 (C++11)

---

## Build

```bash
make configure   # conan install + meson setup
make build       # meson compile
./build/hello    # executa
```

Outros alvos seguem o mesmo padrão do `my_mixr`:

```bash
make install   # copia artefatos para dist/
make package   # gera o pacote Conan deste projeto (conan create .)
make clean     # remove build/ e dist/
make help      # lista os alvos disponíveis (via comentários ## no Makefile)
```

---

## Estrutura

```
mixr-hello/
├── Makefile          # orquestra Conan + Meson
├── conanfile.py       # consome o pacote mixr/1.0.0
├── meson.build         # raiz: resolve mixr_base/mixr_simulation via pkg-config
└── src/
    ├── meson.build
    └── main.cpp        # Station + Simulation, sem EDL, sem models
```

---

## Alvos do Makefile

| Alvo | Ação |
|---|---|
| `make configure` | `conan install` + `meson setup` |
| `make build` | `meson compile` |
| `make install` | copia artefatos para `dist/` |
| `make package` | `conan create .` — gera o pacote deste projeto |
| `make clean` | remove `build/` e `dist/` |
| `make help` | lista os alvos disponíveis (extraído dos comentários `##`) |

---

## Saída esperada

```
=== mixr-hello ===
Reset concluído.
  t = 0s
  t = 1s
  t = 2s
  t = 3s
  t = 4s
=== fim ===
```

---

## O que isso demonstra

- `Station` e `Simulation` podem ser criadas e configuradas diretamente em
  C++ (sem precisar de um arquivo EDL)
- `setSlotSimulation()` é o mesmo mecanismo que o EDL usa por trás dos panos
- `RESET_EVENT` inicializa a árvore de objetos
- `updateTC()` (tempo crítico) e `updateData()` (background) são os dois
  pontos de entrada do ciclo de simulação — aqui chamados manualmente
- `SHUTDOWN_EVENT` + `unref()` é o encerramento limpo
- O fluxo Conan → Meson → Makefile é idêntico ao usado no `my_mixr`: o
  Conan resolve a dependência binária (`mixr`) e gera os arquivos de
  toolchain/pkg-config que o Meson consome via `dependency()`

## Próximo passo

Para ter algo "vivo" na simulação seria necessário adicionar `mixr_models`
e um `Player` com `DynamicsModel` — escopo do projeto `poc-mixr`.
