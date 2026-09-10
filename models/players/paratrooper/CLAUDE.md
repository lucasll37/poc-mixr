# CLAUDE.md — models/players/paratrooper

Complementa o `CLAUDE.md` da raiz e os `.md` deste projeto (`README.md`, `CHANGELOG.md`,
`docs/ARCHITECTURE.md`). Só entra aqui o que não está em nenhum dos dois.

## `.vscode/launch.json` e `meson_options.txt` NÃO refletem o build real

`scripts/models.sh` só reescreve `lib<nome>.so` em `Makefile`/`*.md`. `.vscode/launch.json`
(aponta pra um caminho que nunca existiu) ficou com o texto do template original. Não confie nele
como reflexo do build real (`dist/lib/mixr-plugins/libparatrooper.so`, via Meson/Ninja).

## `Effect::updateTOF()`/`crashNotification()` precisaram das DUAS sobrescritas

Achado lendo o fonte do MIXR ANTES de escrever qualquer física: `Effect` já reduz `maxTOF`
(herdado de `AbstractWeapon`, 60 s) para **10 s** no próprio construtor — insuficiente para um
salto inteiro. E `Effect::crashNotification()` **ignora** `isCrashOverride()` (ao contrário de
`AbstractWeapon::crashNotification()`, que respeita) — setar `crashOverride: true` num `Effect`
puro não impede a cascata de detonação. As duas sobrescritas de `xnative::Paratrooper`
(`updateTOF()` para de incrementar quando `LANDED`; `crashNotification()`/`collisionNotification()`
restauram o respeito a `isCrashOverride()` e trocam a detonação por `setJumpStage(LANDED)`) sanam
os dois problemas — nenhum dos dois seria óbvio sem ler `Effect.cpp`/`AbstractWeapon.cpp`
diretamente.

## `dragIndex` do `Effect` NÃO é utilizável sem calibrar

Default de `Effect` é `0,0006` — equilíbrio de arrasto em `v = g/dragIndex ≈ 16 300 m/s`, puro
balístico. O placeholder do C-130 usa `0,02` (≈490 m/s). Nenhum dos dois é queda livre humana
plausível. `0,18` (≈54,5 m/s) foi escolhido resolvendo a equação de equilíbrio pra trás a partir de
uma terminal alvo — não é chute, mas também não é medido contra um dado de referência real (não
existe "queda livre humana" vendorizada neste repositório para comparar).

## `AbstractWeapon` nasce `INACTIVE` — `AbstractPlayer` nasce `ACTIVE`

Achado que quase passou despercebido: `simulation::AbstractPlayer`'s membro `mode`/`initMode`
default é `ACTIVE` (`AbstractPlayer.hpp`), mas `AbstractWeapon::AbstractWeapon()` SOBRESCREVE os
dois para `INACTIVE` no próprio construtor — proteção padrão contra uma arma "voar sozinha"
enquanto pendurada numa estação. Um `Paratrooper` declarado direto em `players: {}` (sem passar
por um `StoresMgr`) herda esse `INACTIVE` e precisa de `mode: "ACTIVE"` explícito no `.edl` —
documentado em `tests/native/test_paratrooper.cpp::NasceInativoComoTodaArma` e no README.

## `setEulerAngles()` antes de `setVelocity()`, nunca depois

Em `weaponDynamics()`, os estágios `CANOPY`/`LANDED` chamam `setEulerAngles(0,0,heading)` ANTES
de `setVelocity(...)` — a ordem importa porque `setEulerAngles()` muta a matriz de rotação
(`getRotMat()`) que `setVelocity()` usa internamente pra recalcular a velocidade em eixos do
corpo (`velVecBody`). Invertida, a velocidade em eixos do corpo ficaria calculada contra a
atitude ANTIGA (a nariz-baixo herdada do `Effect`), um frame atrasada.

## A suíte `native/` não levanta terreno nenhum — e isso é uma escolha, não uma lacuna

`getAltitudeAglM()` (`Player.inl`) não checa `tElevValid` sozinho — sem terreno (`tElev == 0`,
`Player::reset()`), a AGL devolvida é a altitude MSL crua. A bancada de `tests/native/
test_paratrooper.cpp` usa exatamente essa propriedade: `setAltitude(x)` já É "AGL = x" ali, sem
precisar montar um `SrtmHgtFile`/`WorldModel` com terreno de verdade. O slot `requireTerrain` de
`ParatrooperBtBehavior` (default ligado) existe para o CENÁRIO real, não para a bancada — os
testes que exercitam `ParatrooperBtBehavior` diretamente desligam o gate.

## Registrar uma classe nova em `xnative/factory.cpp` exige sincronizar 3 listas à mão

`src/xnative/factory.cpp` — o `if/else` de `factory()`, `NOMES[]` e `METAS[]` têm que ficar em
sincronia; nada no COMPILADOR força isso, só o registro de plugin em runtime (que recusa a carga
se divergirem). Rodar `python3 tests/guard/check_colisao_fabrica.py` (raiz) depois de qualquer
mudança aqui — nomes de fábrica são globais ao PROCESSO, não por-plugin.
