# CLAUDE.md — models/players/template

Complementa o `CLAUDE.md` da raiz e os `.md` deste projeto (`README.md`, `CHANGELOG.md`,
`docs/ARCHITECTURE.md`, `docs/PRIMEIROS-PASSOS.md`). Só entra aqui o que não está em nenhum dos
dois.

## `.vscode/launch.json` e `meson_options.txt` NÃO são tocados por `make new-model`/rename manual

`scripts/models.sh` só reescreve `lib<nome>.so` em `Makefile`/`*.md` (filtro `find -name Makefile
-o -name '*.md'`). `.vscode/launch.json:11-12` (aponta pra um caminho `src/build/Debug/outDebug`
que nunca existiu — o build real é `dist/lib/mixr-plugins/lib*.so` via Meson/Ninja) e
`meson_options.txt:1` (descrição ainda diz "deste template") ficam com o texto do template
original mesmo depois de copiado/renomeado. Não confie no `launch.json` como reflexo do build
real.

## O exemplo lê `getAltitudeM()` porque é o único slot presente em QUALQUER `Player`

`include/ubf/ExampleState.hpp:44-49` — a escolha não é arbitrária: `Player::getAltitudeM()` existe
mesmo em players que não são veículos aéreos, então o exemplo compila sem depender de subclasse.

## Semântica do Schmitt trigger de exemplo: `>=` nos dois limiares, degenera se `onValue == offValue`

`include/domain/ExampleThreshold.hpp:33-36` / `src/domain/ExampleThreshold.cpp:8-12`:
`engaged ? value >= offValue : value >= onValue`. Os quatro casos de borda estão cobertos em
`tests/domain/test_ExampleThreshold.cpp:8-41`. Quem copiar a regra para outro domínio (ex.:
velocidade, combustível) precisa saber que a convenção deste exemplo é inclusive nos dois lados.

## Sem declarar os slots no `.edl`, o exemplo ENGAJA com quase qualquer leitura

Defaults de `ExampleThreshold` são `onValue=1.0`, `offValue=0.0` (metros) — esquecer de declarar
`onValue:`/`offValue:` no `.edl` não dá erro, só faz o comportamento parecer sempre "ENGAGED".
Já existe como comentário no slottable (`include/ubf/ExampleBehavior.hpp:22-24`), mas nenhum `.md`
avisa disso.

## A suíte de testes NÃO cobre a camada `ubf/` — só `domain/` e a forma do `.so`

`tests/meson.build:19-42` só tem `test-domain` (sobre `ExampleThreshold` puro) e `check_contract.sh`
(forma do `.so`). Não há teste que instancie `ExampleState`/`ExampleBehavior`/`ExampleAction` e
confirme que `genAction()` emite o rótulo certo a partir de uma leitura — quem copiar o template
herda ZERO cobertura na integração percepção→regra→rótulo.

## Registrar uma classe nova em `xnative/factory.cpp` exige sincronizar 3 listas à mão

`src/xnative/factory.cpp:14-19` — o `if/else` de `factory()`, `NOMES[]` e `METAS[]` têm que ficar
em sincronia; nada no COMPILADOR força isso, só o registro de plugin em runtime (que recusa a
carga se divergirem).
