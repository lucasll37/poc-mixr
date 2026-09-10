# CLAUDE.md — models/players/C-130

Complementa o `CLAUDE.md` da raiz e os `.md` deste projeto (`README.md`, `CHANGELOG.md`,
`docs/ARCHITECTURE.md`). Só entra aqui o que não está em nenhum dos dois.

## `.vscode/launch.json` e `meson_options.txt` NÃO foram tocados na geração do scaffold

`scripts/models.sh` só reescreve `lib<nome>.so` em `Makefile`/`*.md`. `.vscode/launch.json`
(aponta pra um caminho que nunca existiu) e `meson_options.txt` (descrição ainda diz "deste
template") ficam com o texto do template original. Não confie neles como reflexo do build real.

## O motor não produzia empuxo efetivo — CORRIGIDO, não redescobrir

**Histórico**: a primeira versão deste modelo copiou `t56.xml` (`<turbine_engine>`, calcula empuxo
direto) pareado com `t56_prop.xml` (um `<propeller>`, que espera potência/HP) — um descompasso de
tipo **no próprio dado JSBSim vendorizado**, não introduzido por este modelo. Sintoma: velocidade
decaía continuamente independente do ganho do autothrottle (testado com bias 0.60/0.85/5.0 —
resultado de velocidade praticamente idêntico nos três, embora o consumo de combustível mudasse
com o bias, provando que o comando de manete chegava ao motor). Rodando tempo suficiente
(~160 s), isso terminava em **estol de verdade e colisão com o terreno** — não descoberto pelos
testes curtos da primeira versão, só ao rodar `sandbox/C-130-6DOF` (rota mais longa) e
`c130-airdrop` por mais tempo.

**Corrigido**: `data/jsbsim/engine/t56_prop.xml` foi removido e o thruster de cada um dos 4
motores em `C130.xml` trocado para `direct` — mesmo padrão já comprovado em
`models/players/A-4/data/jsbsim/aircraft/A4/A4.xml` (`J52`, também `<turbine_engine>`, também
pareado com `<thruster file="direct">`). Não redescobrir testando mais valores de ganho de
autothrottle se a velocidade não sustentar de novo — a causa provável é a mesma classe de bug
(motor/thruster incompatível), não calibração de PID. Ver o achado completo no cabeçalho de
`data/jsbsim/aircraft/C130/c130ap.xml`.

Ganhos PID/lei de subida/limites do `Autopilot` continuam chute inicial (herdados de
`c310ap.xml`/`a4ap.xml` por semelhança de regime) e ainda têm folga/atraso perceptível ao seguir
mudanças de altitude — funcional e seguro com margem de terreno generosa (ver
`src/poc/c130-airdrop/configs/scenario_c130_airdrop.edl.in`, perfil achatado em 1.400 m), mas não
uma calibração fina.

## `Effect` não é construível via EDL puro — precisou de `ParatrooperPlaceholder`

`mixr::models::factory.cpp` despacha `"Chaff"`/`"Decoy"`/`"Flare"`, nunca `"Effect"` bare — um
`( Effect ... )` solto no EDL não constrói nada, mesmo `Effect` sendo uma classe concreta
(`IMPLEMENT_SUBCLASS(Effect, "Effect")` só registra o nome, não garante despacho por ele). Por
isso `ParatrooperPlaceholder` existe como subclasse própria, mesmo sendo trivial.
