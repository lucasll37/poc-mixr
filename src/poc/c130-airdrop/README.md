# `c130-airdrop` — navegação real + liberação de paraquedista

## O que esta poc demonstra

O modelo `models/players/C-130` rodando de ponta a ponta: uma aeronave C-130 navegando de verdade
por `Route`/`Steerpoint` nativo (não patrulha decorativa — a árvore de um nó `Navigate` lê a
guiagem que o framework já calcula a cada frame e comanda o `Autopilot` de verdade) e liberando um
paraquedista a cada volta completa do circuito, no steerpoint `wp2`.

Duas propriedades a observar rodando:

1. **Navegação nativa**: o `bt=` da linha de status/dashboard mostra `NAV` (não `--`), `dec=`
   avança, e a aeronave de fato segue os três steerpoints em loop (`wrap:true`).
2. **Liberação genérica**: até 4 paraquedistas (uma estação por volta) aparecem no Tacview saindo
   de `wp2` — cada um é um `Paratrooper` de produção (`models/players/paratrooper`, FSM completa:
   queda livre, abertura de paraquedas, pouso), liberado por `C130ActionParatrooperRelease` sem
   nenhuma referência a uma classe concreta de paraquedista (a busca é só por `Player::getType()`
   == `"PARATROOPER"`). A 5ª volta em diante não libera nada — comportamento esperado ("acabou a
   carga"), não bug.

## Rodar

```bash
./build/app/src/app -folder src/poc -scenario c130-airdrop
```

Ou em passo fixo, sem TUI:

```bash
./build/app/src/app -f src/poc/c130-airdrop/configs/scenario_c130_airdrop.edl.in -deterministic 30000
```

Tacview na porta **1241** (1234 flight, 1235 bandit, 1237 python-flight/rl, 1238 onnx-policy, 1239
built-in_mixr_1, 1240 full-systems-nav — próxima livre).

## Por que esta poc não entra na lista `pocs` de `tests/meson.build`

Mesmo precedente de `built-in_mixr_1`/`full-systems-nav`: as suítes `scenario`/`memory` daquela
lista derivam fixtures via `tests/scenario/make_fixture.py` e afirmam sobre os rótulos
`EVADE`/`SUPPORT`/`RTB` — vocabulário de combate que este cenário não tem (`bt=` aqui só assume
`NAV`). Determinismo é conferido direto:

```bash
tests/determinism/check_determinism.sh ./build/app/src/app c130-airdrop 2000 '' \
   src/poc/c130-airdrop/configs/scenario_c130_airdrop.edl.in
```

## Achado corrigido — motor não produzia empuxo efetivo (não redescobrir)

**Histórico, já corrigido.** O par motor/thruster vendorizado originalmente copiado para
`data/jsbsim/engine/` deste modelo (`t56.xml`, `<turbine_engine>` — calcula empuxo direto — pareado
com `t56_prop.xml`, um `<propeller>` — espera potência/HP) era um descompasso de tipo do próprio
dado JSBSim vendorizado (Aero-Matic), não algo introduzido por este modelo. Sintoma medido: o
combustível era consumido normalmente com o throttle (prova de que o comando de manete chegava ao
motor), mas o empuxo efetivo que chegava na célula ficava perto de zero — a aeronave desacelerava
sob arrasto continuamente até **estolar de verdade** (rolagem divergindo sem limite, AGL caindo até
quase encostar no terreno, ~160 s simulados). **Corrigido** trocando o thruster para `direct`
(mesmo padrão já comprovado em `models/players/A-4/data/jsbsim/aircraft/A4/A4.xml`, cujo `J52`
também é `<turbine_engine>` pareado com `<thruster file="direct">`) — ver o comentário completo no
cabeçalho de `models/players/C-130/data/jsbsim/aircraft/C130/c130ap.xml` e no bloco `<propulsion>`
de `C130.xml`.

## O que ainda precisa de calibração fina (não impede o voo, hoje é só imprecisão)

**Rumo e altitude funcionam, medido rodando 600 s simulados sem crash** (`bt=NAV`, `dec=`
avançando, os 4 paraquedistas liberáveis em `wp2` a cada volta — um efetivamente liberado e
confirmado via o contador de instâncias do próprio MIXR). A velocidade agora **se recupera** em vez
de decair sem limite (ex.: 73 → 87 kt observado no meio do circuito), mas ainda oscila mais do que
um autopilot bem calibrado deveria, e a malha de altitude segue o comandado com alguma folga/atraso
(undershoot de até algumas centenas de metros em transições, por isso os três steerpoints deste
cenário foram alçados para **1.400 m** — perfil achatado, sem descida/subida, dando margem de
terreno generosa enquanto os ganhos não são refinados — em vez do perfil original 1200/900/1200 m
com "piso do perfil" em `wp2`, que dependia de a malha de altitude reagir rápido o bastante para não
raspar o relevo local ali perto). Os limites do `Autopilot`
(`maxRateOfTurnDps`/`maxBankAngle`/`maxClimbRateMps`/...) e os ganhos de `c130ap.xml` continuam
primeira estimativa — funcionais e seguros com a folga atual, mas não uma calibração fina.

## Ver também

- `models/players/C-130/README.md`/`docs/ARCHITECTURE.md` — o modelo que esta poc exercita
- `src/poc/full-systems-nav/README.md` — o mesmo padrão de navegação nativa, no modelo A-4
