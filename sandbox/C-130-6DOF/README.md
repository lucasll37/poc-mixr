# C-130-6DOF — um C-130, dinâmica JSBSim (6-DOF), a mesma figura-de-oito de A4-6DOF

Reaproveita a **geometria de rota** de `sandbox/A4-6DOF` (a figura-de-oito fechada de 20
steerpoints, mesmo perfil de altitude 4.000–15.000 ft, mesmo ponto de entrada) com o modelo
**C-130** (`models/players/C-130`) no lugar do "player máximo" A-4 — ver
`sandbox/A4-6DOF/README.md` para a construção completa da rota (decágono, varredura de terreno,
por que as pernas são de 14 km). Este README só documenta o que **muda**.

```bash
./build/app/src/app -folder ./sandbox -scenario C-130-6DOF        # tempo real, Tacview 1234
./build/app/src/app -folder ./sandbox -scenario C-130-6DOF -deterministic 2000
```

## O que é igual a A4-6DOF

- As 20 posições/altitudes/`stptType`/`sca`/`magvar`/`pta` dos steerpoints, byte a byte.
- `to:1 / autoSequence:true / autoSeqDistance:(NauticalMiles 2.0) / wrap:true` — as mesmas
  pernas de 14 km.
- `initXPos`/`initYPos`/`initHeading` (54°) — o mesmo ponto de entrada, 4 km antes de `wp1`,
  sobre a perna `wp10→wp1`, sem transiente de junção.
- Terreno (mesmo tile SRTM, mesma referência lat/lon).
- Porta Tacview **1234** — mesma convenção da família `sandbox/A4-*DOF` (nenhum dos cenários
  desta família roda ao mesmo tempo que outro).

## O que muda

**Um C-130 só, não oito empilhados.** Sem a separação vertical de +1.000 ft/aeronave nem o
esquema de velocidade-calibrada-por-altitude que a pilha de A4-6DOF precisa para não se
desmanchar — isso só faz sentido para formação; aqui não há o que desmanchar.

**Sem o "player máximo".** Nada de `Gimbal`/`Antenna`/`CommRadio`/`Iff`/`SigSwitch`/
`IrSignature`/`OnboardComputer`-com-4-track-managers/`StoresMgr`-de-11-estações/
`CollisionDetect` — essa aparelhagem é do A-4 "player máximo" (`src/poc/built-in_mixr_1`), não
do C-130 (que hoje só tem `JSBSimModel` + `Autopilot` + `Navigation` + `StoresMgr` de
paraquedista). A montagem do player é a mesma de `src/poc/c130-airdrop`, só voando a rota de
A4-6DOF em vez do circuito de 3 pontos daquela poc.

**As três `Action` originais trocam de sentido:**

| steerpoint | A4-6DOF | C-130-6DOF | por quê |
|---|---|---|---|
| wp5 | `ActionDecoyRelease` | `C130ActionParatrooperRelease` | a única capacidade real deste modelo — "piso do perfil" (4.000 ft) também faz sentido operacional para uma largada |
| wp9 | `ActionImagingSar` | *(nenhuma)* | precisa de um `( Sar )` RfSensor que este player não tem |
| wp15 | `ActionCamouflageType` | *(nenhuma)* | só tem efeito visível com um `( SigSwitch )` multi-modo que este player não tem |

4 estações `"PARATROOPER"` no `StoresMgr` (mesmo padrão de `c130-airdrop`) — cada volta completa
(`wrap:true`, ~2000 s simulados na velocidade nominal) libera mais uma, até a carga acabar na
5ª volta.

**Velocidade comandada deslocada para baixo.** Os 210–245 kt de A4-6DOF viraram 150–185 kt —
mesmo padrão relativo (mais rápido nas pernas de descida, mais devagar nas de subida/cruzamento),
numa faixa plausível para um C-130 (folga sobre o stall ~100–115 kt limpo) em vez de copiar
cegamente números calibrados para o A-4.

**Tacview usa o modelo 3D do C-130**, não o do A-4: `modelMap: { c130: "C-130" }` — confirmado
contra o banco de dados público do próprio Tacview
(`Tacview.DefaultProperties.FixedWing.Military.xml`, github.com/Vyrtuoz/Tacview): a entrada
`FixedWing.C-130` tem `"C-130"` como `ShortName` e como primeiro alias de `Criteria`, resolvendo
para o modelo genérico do Hercules (não há entrada H/J/E separada no banco — só `KC-130`, a
variante-tanque, é quebrada à parte).

## Achado corrigido — motor não produzia empuxo efetivo (histórico, não redescobrir)

Numa rodada anterior, a velocidade decaía continuamente (e a aeronave chegou a **colidir com o
solo** — o par motor/thruster vendorizado originalmente copiado para `data/jsbsim/engine/` deste
modelo, `t56.xml` `<turbine_engine>` + `t56_prop.xml` `<propeller>`, era um descompasso de tipo do
dado JSBSim vendorizado: o motor "rodava" (combustível consumido normalmente) mas o empuxo efetivo
que chegava na célula ficava perto de zero. **Corrigido** trocando o thruster para `direct` (mesmo
padrão já comprovado em `models/players/A-4/data/jsbsim/aircraft/A4/A4.xml`) — ver o cabeçalho de
`c130ap.xml` e `src/poc/c130-airdrop/README.md` para o achado completo.

**Medido depois da correção, nesta rota especificamente** (mais longa que `c130-airdrop`, ~2000 s
por volta contra ~130 s do circuito de 3 pontos): 600 s simulados (frame 30000) sem crash, `dec=`
avançando 1:1 com `frame=`, velocidade estabilizada em torno de 200 kt (não mais decaindo sem
limite), e altitude tracking bem próxima do perfil comandado (~1.217 m contra os ~1.219 m/4.000 ft
esperados perto de `wp5`, a estação mais baixa do perfil) — a folga generosa de terreno que este
perfil já tinha (herdada de `A4-6DOF`, escolhida por varredura de terreno para o A-4) absorveu sem
drama a imprecisão residual da malha de altitude do C-130. Completar uma volta inteira (~2000 s)
não foi verificado até o fim nesta poc, mas a liberação do paraquedista em `wp5` já está provada
funcionando, ponta a ponta, em `src/poc/c130-airdrop`.

## Verificação manual

```bash
python3 src/ui/scripts/edl_lint.py sandbox/C-130-6DOF/configs/scenario_c130_6dof.edl.in
# fábrica desconhecida C130* é esperado -- catálogo estático do lint nunca viu estas
# classes (mesmo caso já documentado para c130-airdrop). edlcheck é a autoridade real:
sed 's/@NUM_TC_THREADS@/1/; s/@RUN_ID@/x/g' sandbox/C-130-6DOF/configs/scenario_c130_6dof.edl.in \
   > /tmp/c130_6dof_check.edl && dist/bin/edlcheck /tmp/c130_6dof_check.edl

./build/app/src/app -folder ./sandbox -scenario C-130-6DOF -deterministic 500
# esperado: bt=NAV, dec= avancando, altitude proxima de 11000 ft (3352 m)

./tests/determinism/check_determinism.sh ./build/app/src/app C-130-6DOF 2000 '' \
    sandbox/C-130-6DOF/configs/scenario_c130_6dof.edl.in    # determinismo com 1, 2 e 4 threads T/C
```

`edlcheck` recusa o `.edl.in` cru com `error while setting slot name: numTcThreads` —
`@NUM_TC_THREADS@`/`@RUN_ID@` só são expandidos em runtime por `app::generateScenario()`, nunca
por `edlcheck` direto — mesmo comportamento de `A4-6DOF`.
