# `missile` — míssil guiado cinemático, exercício de uso idiomático do MIXR

## O que é isto

Um plugin de modelo MIXR (`libmissile.so`) que implementa `( GuidedMissile )`: um míssil
ar-ar guiado por **navegação proporcional de verdade** (não a perseguição simplificada
embutida no `mixr::models::Missile` nativo), sem `dynamicsModel` nenhum — a guiagem e a
dinâmica são os dois *hooks* protegidos que o próprio framework já reserva para isso
(`weaponGuidance()`/`weaponDynamics()`).

Nasceu como exercício (ver `TODO.md` na raiz do repositório): "verificar como um míssil é
modelado em termos de uso idiomático do MIXR". O cenário que o dispara —
`sandbox/A4-6DOF-MISSILE/` — tem um A-4 armado detectando outro A-4 pelo radar e disparando
contra ele; ver o `README.md` daquele cenário para a demonstração fim a fim.

Este diretório começou como cópia de [`models/players/template`](../template/) (via
`make new-model NAME=missile CATEGORY=player`) e foi **podado**, não só preenchido: as
camadas `bt/`/`ubf/` do scaffold de origem saíram inteiras — ver
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) para o porquê.

## Por que não é o modelo antigo de mesmo nome

Já existiu, neste mesmo diretório, um `GuidedMissile` diferente — com `JSBSimModel`
próprio (uma aeronave `"aim1"` vendorizada, de baixa inércia) e guiagem por perseguição
pura com amortecimento (`domain::Guidance::pursuit()`/`slewTowards()`). Foi removido do
repositório junto com o cenário de demo que o carregava. Esta versão **não** reutiliza
aquele fonte (recuperável via `git show <rev>:models/players/missile/...` se algum dia for
preciso comparar) — é uma implementação nova, deliberadamente mais leve: sem aeronave
JSBSim própria para manter, usando os *hooks* cinemáticos que `mixr::models::Missile` já
oferece para exatamente este caso.

## Estrutura

```
missile/
├── include/
│   ├── domain/Guidance.hpp     # a lei de guiagem + a espoleta -- pura, sem MIXR
│   └── xnative/
│       ├── GuidedMissile.hpp   # a classe MIXR (subclasse de mixr::models::Missile)
│       └── factory.hpp         # registro -- so' esta classe
├── src/                        # implementação de cada header acima
├── tests/
│   ├── domain/test_Guidance.cpp    # a lei de PN converge, a espoleta dispara certo
│   └── check_contract.sh           # forma do .so: 1 símbolo T, deps resolvidas
├── docs/ARCHITECTURE.md        # as decisões deste modelo especificamente
├── CHANGELOG.md
├── Makefile                    # build autocontido
└── meson.build                 # UM artefato: libmissile.so
```

## Compilar e testar, sozinho

```bash
# uma vez, na raiz do repositorio:
cd ../../.. && make configure && make sdk

# daqui em diante, so' aqui dentro:
make build            # -> ./dist/lib/mixr-plugins/libmissile.so
make test             # domain/ (a lei de PN + a espoleta) + a forma do .so
make install-host     # copia pra ../../../plugins/ -- ver a proxima secao
```

`make help` lista todos os alvos.

## Publicar para um cenário de verdade carregar

Mesmo fluxo de qualquer modelo deste repositório: `make install-host` (aqui) deposita em
`../../../plugins/`; `cd ../../.. && make install` sincroniza `plugins/` → `dist/`, onde um
cenário de fato procura. Ou, do dia a dia, só `make models && make install` na raiz do
repositório (que já descobre este diretório junto com `A-4`).

## Se você quiser mais contexto

- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — as decisões de arquitetura deste modelo:
  por que cinemático em vez de JSBSim, por que sem árvore de comportamento, a lei de guiagem
  em si.
- [`../../../CLAUDE.md`](../../../CLAUDE.md), seção "O MODELO é um plugin, construído numa
  etapa PRÉVIA" — visão geral de `models/`, o contrato de plugin, e o build orquestrado pelo
  Makefile da raiz.
- [`../../../sandbox/A4-6DOF-MISSILE/README.md`](../../../sandbox/A4-6DOF-MISSILE/README.md)
  — o cenário que carrega este modelo, com o que foi medido rodando.
