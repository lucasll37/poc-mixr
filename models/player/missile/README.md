# `missile` — segundo modelo de exemplo

Projeto Meson **independente** (não sabe nada de `models/player/A4/` nem de `src/`), publicando um
único `Player` novo: `GuidedMissile`, guiado sobre um `JSBSimModel` anexado (física 6-DOF de
verdade — não é um `Player` de brinquedo). Existe como exemplo de "criar um modelo novo" e de
"ativar um player em runtime" (ver [../../CLAUDE.md](../../../CLAUDE.md), seção "Demo: míssil
guiado", para o passo a passo completo: lançamento → detonação → destruição).

Carregado **ao lado** do `flight`, por um segundo `( PluginModule )`, só no cenário de demo
(`src/poc/dis/single-thread/configs/scenario_missile_demo.edl.in`) — `provides:` publica só
`{ GuidedMissile }`, então os cenários de produção (que só carregam `libflight.so`) nunca precisam
mudar.

```
missile/
├── src/
│   ├── plugin.cpp              # a fronteira C -- so um nome, sem indirecao de factory
│   ├── domain/Guidance.{hpp,cpp}   # a lei de guiagem, PURA (sem MIXR)
│   └── xmissile/GuidedMissile.{hpp,cpp}   # o Player -- guia sobre o JSBSimModel
├── tests/domain/                # so domain::pursuit(), sem levantar Station
├── docs/DESIGN.md               # a guiagem em detalhe + a armadilha de inercia/massa
├── CHANGELOG.md                 # o que mudou neste modelo, e por que
├── Makefile                     # build autocontido -- ver abaixo
└── meson.build
```

## Build autocontido

```bash
# uma vez, na raiz do poc-mixr:
cd ../.. && make configure && make sdk

# daqui em diante, so aqui dentro:
cd models/player/missile
make                 # -> ./dist/lib/mixr-plugins/libmissile.so
make test            # roda tests/domain (domain::pursuit(), 5 casos)
make install-host    # sincroniza ./dist para a raiz do poc-mixr
```

`make help` lista os alvos. Mesmo padrão de [`../fixtures/stub/Makefile`](../fixtures/stub/Makefile)
— ver o README dele para o "porquê" de `install-host` ser o único alvo que sai de `./dist`.

## Dependências

Só `mixr_dep` + `sdk_dep` — **não** linka `behaviortree.cpp.asa`: este modelo não decide com
árvore, só guia e voa. É a prova, junto com o `stub`, de que o contrato de plugin não impõe pilha
interna nenhuma.

## Ler também

- [docs/DESIGN.md](docs/DESIGN.md) — a lei de guiagem e a armadilha de inércia/massa
- [CHANGELOG.md](CHANGELOG.md) — o que mudou neste modelo, e por quê
- [../README.md](../../README.md) — visão geral de `models/`, o contrato de plugin, e o build
  orquestrado pelo Makefile da raiz (`make models`)
- [../../CLAUDE.md](../../../CLAUDE.md) — seção "Demo: míssil guiado", a dissecação completa
