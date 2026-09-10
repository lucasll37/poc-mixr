# CLAUDE.md — models/others/Navstar-3

Complementa o `CLAUDE.md` da raiz e os `.md` deste projeto (`README.md`, `CHANGELOG.md`,
`docs/ARCHITECTURE.md`). So' entra aqui o que nao esta em nenhum dos dois.

## `.vscode/launch.json` NAO reflete o build real

Mesmo texto herdado de `models/players/template/` -- aponta pra um caminho que nunca existiu
(`src/build/Debug/outDebug`). O build real e' `dist/lib/mixr-plugins/libNavstar-3.so`, via
Meson/Ninja. `scripts/models.sh` so' reescreve `lib<nome>.so` em `Makefile`/`*.md`, nao em
`.vscode/`.

## `Player::setAltitude()`/`setPosition()` exigem `useCoordSys` resolvido E um `WorldModel`

Achado escrevendo `tests/native/`: um `SpaceVehicle` recem-construido nasce com `useCoordSys ==
CS_NONE` (`Player.hpp:950`) -- `setAltitude()` so' faz alguma coisa se `useCoordSys` for
`CS_LOCAL`/`CS_GEOD`/`CS_WORLD`, e so' `Player::reset()` resolve isso (`Player.cpp:465-479`, o
ramo `else` cai em `CS_LOCAL` quando nenhum slot `init*` foi declarado). Alem disso, o ramo
`CS_LOCAL` de `setAltitude()`/`setPosition()` chama `getWorldModel()->getMaxRefRange()` SEM checar
nulo -- um `SpaceVehicle` sem `container()` nenhum crasha. `tests/native/test_navstar3.cpp`
sempre passa por um `BenchWorld` + `reset()` antes de tocar altitude/posicao a mao.

## `Player::setGeocVelocity()` NAO tem parametro `slaved`

So' `setGeocPosition()`/`setPosition()`/`setPositionLLA()`/`setAltitude()` tem essa sobrecarga
(confirmado no header, `Player.hpp:730,721,715,808-809`). A velocidade nunca compete com
integrador nenhum -- e' so' um campo de leitura (`vp`/`gndSpd`/telemetria), entao nao precisa de
flag de "travado".

## `spd=` no dump e' velocidade RELATIVA AO REFERENCIAL ECEF (fixo-na-Terra), nao inercial

Ver `docs/ARCHITECTURE.md`, secao dedicada -- registrado aqui tambem porque e' o tipo de numero
que parece errado a quem compara contra a formula ingenua `v=sqrt(mu/a)` sem pensar no referencial.

## `xboard` e' GLOBAL AO PROCESSO -- bancadas de teste precisam de ID UNICO por instancia

`libs/xboard::Board.hpp` e' chaveado por `player->getID()`, e o `gtest` roda todos os `TEST()` de
um binario no MESMO processo. Um `Bench` com ID fixo repetido entre casos de teste faz o contador
de decisoes "vazar" de um teste pro seguinte (medido: um teste que rodou 432 frames sobre ID 9601
deixou `xboard::get(9601).decisions == 432` para o PROXIMO teste que reusasse o mesmo ID, que
esperava `== 1`). `tests/native/test_navstar3.cpp::Bench` usa um contador atomico
(`std::atomic<int> nextId`) para nunca repetir ID entre instancias.

## Testar via `./dist/bin/node` NAO chama `TacviewOutput::publishIdentities()`

Achado comparando: rodar `src/poc/navstar3-orbit` via `node` (o runner headless, ver `src/node/
README.md`) grava `Type=Misc,Color=Violet` no `.acmi` (os DEFAULTS de fallback por major-type/
side) em vez de `Type=Misc+Satellite,Color=Blue` (o `typeMap`/`colorMap` do proprio cenario) --
`REID_PLAYER_DATA` traz `PlayerId` PARCIAL (so' `id`/`name`, sem `ac_type`/`side`,
CLAUDE.md raiz, secao xtacview), e so' `./app` (`DeterministicRun.cpp`/`DashboardLoop.cpp`) chama
`publishIdentities()` para pre-popular o cache ANTES do primeiro `REID_PLAYER_DATA`. Rodando via
`./app -deterministic`, o mesmo cenario grava a identidade certa desde a primeira linha. Nao e'
um bug do modelo nem do cenario -- e' uma lacuna conhecida do runner `node`, que nao reaproveita
nada de `app/` de proposito (ver `src/node/README.md`).

## `generate_edl_catalog.py`/`edl_lint.py` so' enxergavam `models/players/*` -- generalizado aqui

Achado ao rodar o hook `check-edl-lint.sh` contra o cenario deste modelo: `dispatch_factory_cpp_
paths()` em `src/ui/scripts/generate_edl_catalog.py` so' fazia glob de
`models/players/*/src/xnative/factory.cpp` -- as classes deste modelo (o PRIMEIRO real fora de
`models/players/`) nunca apareciam no catalogo, e o lint acusava "fabrica desconhecida" para
`Navstar3State`/`Navstar3BtBehavior`/`Navstar3AgentTC` mesmo com o `.edl` correto. `origin_of()`
tinha o mesmo problema na direcao inversa: rotulava qualquer classe sob `models/<categoria>/
<nome>/` como `plugin:<categoria>` (ex.: `plugin:others`), nao `plugin:<nome>`. Generalizado com
uma constante `MODEL_CATEGORY_DIRS = {"players","systems","others"}` (espelho do `CATEGORIA_DIR`
de `scripts/models.sh`) usada nos dois lugares. Regenerado via `node src/ui/scripts/build.js`
(pipeline completo, `edl_catalog.generated.json`/`preset.json`/`edl-builder.html`) -- o mesmo
comando que qualquer modelo novo precisaria rodar caso seja o primeiro a expor uma lacuna
parecida numa categoria diferente.
