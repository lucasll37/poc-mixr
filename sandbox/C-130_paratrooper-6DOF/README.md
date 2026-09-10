# C-130_paratrooper-6DOF — a integração de verdade: C-130 largando 30 paraquedistas reais

A integração entre `models/players/C-130` e `models/players/paratrooper` que `src/poc/c130-airdrop`
deixou para depois: **um C-130** (dinâmica JSBSim 6-DOF) que, no **segundo ponto de navegação**,
libera **30 paraquedistas de verdade** (a classe `Paratrooper` real — queda livre, paraquedas,
pouso — não o placeholder `C130ParatrooperPlaceholder`), com **1,5 segundos de intervalo** entre
uma liberação e a próxima.

```bash
./build/app/src/app -folder ./sandbox -scenario C-130_paratrooper-6DOF                       # tempo real, Tacview 1234
./build/app/src/app -folder ./sandbox -scenario C-130_paratrooper-6DOF -deterministic 25000  # headless
```

## O mecanismo novo — `C130ActionParatrooperStick`

`models/players/C-130` ganhou uma classe irmã de `C130ActionParatrooperRelease` (que libera **uma**
estação por cruzamento de steerpoint): `C130ActionParatrooperStick` libera **várias**, espaçadas no
tempo, a partir de **um único** cruzamento — o termo militar real para o grupo que salta em
sequência rápida na mesma passagem da aeronave.

**Nenhum mecanismo de tempo foi inventado.** `mixr::models::Action` já tem exatamente o que se
precisa: `trigger()` roda uma vez (no cruzamento); se `isInProgress()` continuar verdadeiro depois
disso, `OnboardComputer::actionManager()` chama `process(dt)` a cada ciclo de **fundo** até
`isCompleted()`. `ActionParatrooperStick::trigger()` libera a primeira estação na hora e permanece
em progresso; `process()` acumula tempo simulado e libera mais uma a cada `interval` decorrido, até
`count` liberações. A busca em si (`releaseNextStoreOfType()`) foi extraída de
`ActionParatrooperRelease` para as duas compartilharem — ver
`models/players/C-130/include/xnative/ActionParatrooperStick.hpp` para o detalhe completo.

**As 30 estações são `( Paratrooper )`, não `( C130ParatrooperPlaceholder )`** — os dois plugins
(`libC-130.so` e `libparatrooper.so`) carregam juntos no mesmo processo, e a liberação genérica por
`type:` (`ActionParatrooperStick` nunca faz `dynamic_cast` numa classe concreta, só compara
`Player::getType()`) alcança a classe real do outro plugin sem nenhuma mudança de C++ em nenhum dos
dois — exatamente o contrato que `models/players/C-130/docs/ARCHITECTURE.md` já prometia.

## A rota — nova e simples, por decisão explícita

Diferente de `sandbox/C-130-6DOF` (figura-de-oito de 20 pontos): aqui só 3 steerpoints em linha
reta para leste — wp1 (subida inicial), wp2 (**o ponto de largada**, comandado 15.000 ft) e wp3
(egresso), com `wrap:true` de volta a wp1.

## Achado medido rodando — por que a rota final não é a primeira tentativa

A primeira versão desta rota assumia `maxClimbRateMps: 9.0` do `Autopilot` como taxa de subida
real e ia direto de 1.200 m até 15.000 ft. **Rodando**, isso revelou dois problemas, os dois já
corrigidos:

1. **A subida real fica bem abaixo do limitador genérico** — o mesmo achado que
   `sandbox/C-130-6DOF/README.md` já registra ("os limitadores do Autopilot são INERTES com
   `( JSBSimModel )` — quem manda é o autopilot do próprio airframe, `c130ap.xml`"). Medido: ~1,5
   m/s nos primeiros 100 s de subida, só passando de ~6 m/s perto dos 480-500 s.
2. **A rota original ultrapassava a borda do único tile SRTM vendorizado.** A borda leste fica a
   ~49,4 km da referência (-22,25, -42,48) — medido a partir do próprio arquivo `.hgt`, não do
   README do tile. Com wp2 a 55 km, a aeronave voava para além da borda e `elev=` caía para
   exatamente `0.000000000` por dezenas de frames seguidos — o sintoma exato já documentado no
   `CLAUDE.md` raiz, seção "Terreno" ("fora da célula do tile... não é guarda de cobertura").

**Corrigido**: a aeronave agora começa mais alta (3.200 m, não 1.200 m — encurta a subida
necessária de 3.372 m para 1.372 m) e a rota inteira fica dentro de 42 km (~7,4 km de folga sobre a
borda de 49,4 km).

## O que foi medido rodando, depois da correção (25.000 frames, `-threads 2`)

- **Nenhum `elev=0.000000000`** em nenhum dos 250 dumps do C-130 — a rota inteira fica dentro do
  tile.
- **A largada dispara certinho ao cruzar wp2**: primeiro paraquedista (`W10001`) aparece no
  `mission.jsonl` em t=314,88 s — o MESMO instante em que o C-130 cruza `eastM≈30000` (wp2).
- **Altitude real da aeronave ao cruzar wp2: ~3.775-3.800 m (~12.400-12.500 ft)** — abaixo do
  comandado (15.000 ft), mas uma subida substancial a partir dos 10.500 ft de entrada. `wp2` é o
  valor **comandado**, não um teto travado — a liberação dispara pela distância geométrica ao
  steerpoint (`autoSeqDistance`), não por ter alcançado a altitude exata. Alongar ainda mais a
  rota aproximaria mais o valor real do comandado, ao custo de um cenário mais longo — não feito
  aqui.
- **Os 30 disparam com intervalo de 1,50 s entre um e o próximo, os 30** (medido pelos timestamps
  de primeira aparição de cada `W1000N`, de `W10001` a `W10030`, no `mission.jsonl`: primeiro em
  t=314,88 s, último em t=358,36 s) — as 29 lacunas entre liberações saem `1.5` exatas (a primeira
  mede `1.48`, artefato do passo de amostragem de 2 s do `msgFeed`, não do disparo em si) — a
  precisão do acumulador de tempo simulado de `ActionParatrooperStick::process()`.
- **Todos os 30 — confirmado nos 30, não por amostragem** — passam por `PRE_RELEASE→ACTIVE` (o
  ciclo nativo de liberação de arma, `AbstractWeapon::dynamics()`) e terminam a corrida com
  `altAglM < 2 m` (dentro do limiar `groundAgl`), `damage=0`, `crashedFlag=0`, `killedFlag=0` nos
  30 — pousaram em segurança, sem nenhum crash genérico disparando a cascata de detonação que
  `models/players/paratrooper` foi desenhado para evitar (ver
  `models/players/paratrooper/docs/ARCHITECTURE.md`).
- **A aeronave completa a curva de retorno** (`wrap:true`) sem sair do tile — banco sustentado de
  ~27-28° durante a curva, medido sem tocar a borda leste (pico em ~46,2 km, ~3,2 km de folga).

## Tacview

Dois `typeMap`/`colorMap`/`modelMap` — um para `c130` (mesmo de `sandbox/C-130-6DOF`), outro para
`PARATROOPER` (mesmo de `src/poc/paratrooper-drop`). Sem o segundo, os 30 apareceriam com o
fallback genérico de `WEAPON` ("Weapon+Missile").

## Por que o `-deterministic` não mostra os paraquedistas no dump `frame=`

**Limitação conhecida, não introduzida por esta poc**: `app::runDeterministic()` captura a lista de
players **uma vez**, antes do laço — um player criado em runtime (como um flyout de arma liberada)
nunca entra nesse snapshot. O `msgFeed` (`libs/xmsg`) é o mecanismo usado aqui porque ele **não**
depende de snapshot nenhum — reamostra `getWorldModel()->getPlayers()` a cada ciclo, pegando quem
nasceu depois do início. Ver o `mission_*.jsonl` em `data/messages/` para os 30 paraquedistas.

## Verificação manual

```bash
python3 src/ui/scripts/edl_lint.py sandbox/C-130_paratrooper-6DOF/configs/scenario_c130_paratrooper_6dof.edl.in
# fábrica desconhecida C130*/Paratrooper* e esperado -- catalogo estatico do lint nunca viu estas
# classes antes de "node src/ui/scripts/build.js" regenerar; edlcheck e a autoridade real:
sed 's/@NUM_TC_THREADS@/1/; s/@RUN_ID@/x/g' sandbox/C-130_paratrooper-6DOF/configs/scenario_c130_paratrooper_6dof.edl.in \
   > /tmp/c130_para_6dof_check.edl && dist/bin/edlcheck /tmp/c130_para_6dof_check.edl

./build/app/src/app -folder ./sandbox -scenario C-130_paratrooper-6DOF -deterministic 25000
# esperado: bt=NAV no c130, dec= avancando, elev= nunca exatamente 0.000000000

grep -o '"player":"W1[0-9]*"' sandbox/C-130_paratrooper-6DOF/data/messages/mission_*.jsonl | sort -u
# esperado: W10001 .. W10030 -- os 30 liberados
```

## Determinismo e regressao -- verificado, nao so' assumido

```bash
bash tests/determinism/check_determinism.sh ./build/app/src/app c130-paratrooper-6dof 2500 "" \
   sandbox/C-130_paratrooper-6DOF/configs/scenario_c130_paratrooper_6dof.edl.in
# OK -- 1, 2 e 4 threads T/C batem byte a byte, e uma repeticao de 4 threads confirma
# reprodutibilidade na MESMA configuracao

# regressao nos DOIS cenarios cujo provides: mudou (a classe nova entrou no MESMO .so):
bash tests/determinism/check_determinism.sh ./build/app/src/app c130-airdrop-regress 1500 "" \
   src/poc/c130-airdrop/configs/scenario_c130_airdrop.edl.in
bash tests/determinism/check_determinism.sh ./build/app/src/app c130-6dof-regress 1500 "" \
   sandbox/C-130-6DOF/configs/scenario_c130_6dof.edl.in
```

Os tres passaram. `meson test -C build` (suite completa do HOST) tambem passou -- 68/68, sem
nenhuma regressao nas suites `scenario`/`memory`/`determinism`/`plugin`/`guard` existentes. As
duas suites do MODELO (`cd models/players/C-130 && make test`, `cd models/players/paratrooper &&
make test`) passam 5/5 cada.

**Confirmado nos 30 paraquedistas, nao por amostragem** (rodando um `-deterministic 25000` limpo e
lendo o `mission_*.jsonl` inteiro): os 30 aparecem (`W10001`..`W10030`), as 29 lacunas entre
liberacoes consecutivas saem `1.5` segundos exatos (a primeira mede `1.48`, artefato do passo de
amostragem de 2s do `msgFeed`), e os 30 terminam com `altAglM < 2 m`, `damage=0`, `crashedFlag=0`,
`killedFlag=0` -- pouso seguro nos 30, sem excecao.
