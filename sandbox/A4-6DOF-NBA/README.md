# A4-6DOF-NBA — um A-4 só, voo reto a 300 ft AGL acompanhando o terreno real por ~288 km

Ver o índice dos cenários deste `sandbox/` em [`sandbox/README.md`](../README.md).

NBA = *Nap-of-the-earth, Baixa Altitude*. Ao contrário dos outros cenários desta pasta, este
não existe para explorar frota, dinâmica ou árvore de comportamento — existe para **validar,
rodando, o carregamento correto da elevação do terreno** (`libs/xterrain::MultiTileTerrain`,
ver a seção própria no `CLAUDE.md` da raiz), sobre dado **real** e vendorizado (os tiles SRTM1
de `shared/data/terrain/srtm/`), atravessando de verdade a fronteira entre dois tiles.

```bash
./build/app/src/app -folder ./sandbox -scenario A4-6DOF-NBA        # tempo real, Tacview 1234
./build/app/src/app -folder ./sandbox -scenario A4-6DOF-NBA -deterministic 60000
```

**Se uma corrida `-deterministic` bem longa (dezenas de milhares de frames) morrer com
`BUS on unknown address ... em mixr::base::Component::tcFrame()`**, não é deste cenário: é a
mesma corrida de dados rara e já documentada em `tests/README.md` ("Capturar evidência de um
crash" — um worker do pool de tempo crítico percorrendo a lista de players por um ponteiro de
lixo, sob contenção de CPU). Observado aqui em ~1 a cada poucas corridas de 40-90 mil frames,
nunca no mesmo ponto do corredor duas vezes — é rodar de novo, não um defeito deste `.edl.in`.

## A pergunta que este cenário responde

> Um avião que voa reto, por uma distância grande o bastante para atravessar mais de um tile
> SRTM, recebe uma leitura de elevação **contínua e correta** o tempo todo — inclusive
> exatamente na fronteira entre os dois arquivos — ou o carregamento multi-tile "escorrega" em
> algum ponto?

A resposta, medida rodando `-deterministic 60000` e lendo a telemetria (`data/messages/
mission_*.jsonl`, campo `terrainElevM`) e o dump determinístico: **sim, contínua e correta**,
do início do corredor até ~196 km — o trecho que atravessa a fronteira real `S23W044`/`S23W043`.
Depois disso, o cenário tropeça num bug **diferente e não relacionado** (native, do MIXR) — ver a
seção própria abaixo, que é a parte mais interessante do que se descobriu construindo isto.

## ARMADILHA CONFIRMADA E CORRIGIDA — o piso ABSOLUTO escondia o acompanhamento de contorno

**Relatado observando a vista Lateral do `./app`: a aeronave não acompanhava o contorno do solo
— voava aparentemente nivelada por boa parte do corredor.** Investigado com a telemetria real
(`altMslM` contra `terrainElevM`), não por inspeção de código isolada: não era bug de
`MultiTileTerrain`/carregamento de tile (a elevação lida estava correta e variando de verdade) nem
da vista Lateral — era o **piso absoluto** de `domain::terrainFloorM()`
(`max(minSafeAltitude, terreno + terrainClearance)`, `TerrainFloor.hpp`) dominando o `max()` na
maior parte do corredor.

Até esta correção, `minSafeAltitude` era a constante C++ `MIN_SAFE_ALT_M = 200.0`, fixa,
compartilhada por **todo** cenário que usa este modelo (`BtBehavior.cpp`). Com
`terrainClearance: ( Feet 300 )` (~91,44 m), o termo do terreno só vence os 200 m quando a
elevação real passa de ~108,6 m — e esta faixa de Serra do Mar descendo para a baixada costeira
fica, na maior parte do trajeto, **abaixo** disso. Medido numa corrida completa: o piso de 200 m
dominava em **90% das amostras** — a aeronave voava nivelada a 200 m MSL constante em vez de
acompanhar as subidas/descidas reais do relevo, exatamente o platô observado na vista Lateral.

**A correção**: `MIN_SAFE_ALT_M` virou o slot `minSafeAltitude` de `( BtBehavior )`
(`models/players/air/A-4/src/ubf/BtBehaviorSlots.cpp`), com **default 200 m** — todo cenário que
não declarar o slot (inclusive `flight`/`patrol`/os demais `sandbox/A4-*`) continua byte a byte
igual a antes. Este cenário passou a declarar `minSafeAltitude: ( Meters 0 )`, por pedido
explícito ("despreze a altitude de segurança, voe o que for configurado para voar"): o piso
absoluto vira só "nunca abaixo do nível do mar", e o termo do terreno decide sozinho o tempo quase
todo.

**Medido depois da correção** (mesma corrida, `-deterministic 60000`): o piso de 0 m domina em só
**12,8% das amostras** — e as 154 amostras onde isso acontece são **exatamente** os dois trechos já
documentados abaixo onde o bug nativo de decodificação SRTM devolve elevação ~-32700 m (nenhuma é
terreno real genuinamente abaixo de -91,44 m). Ou seja: fora do bug nativo já conhecido, a
aeronave agora acompanha o contorno real do terreno em **100%** do corredor, com `altAglM`
oscilando perto dos 300 ft pedidos (tipicamente 70-170 m, a variação vindo do atraso físico normal
de subida/descida do Autopilot, não de um piso artificial). Durante os dois trechos bugados, a
altitude comandada agora fica perto de **0 m MSL** (nível do mar) em vez de travar em 200 m —
consequência direta de "desprezar a altitude de segurança": o piso residual de 0 m ainda evita que
o avião mergulhe até o `( Meters -2000 )` cru do `wp1` (ver a seção abaixo), mas não mais o mascara
em 200 m como antes.

## Como o cenário funciona — só EDL, zero C++ novo

Um único player (`a4`, mesmo modelo/plugin `A-4` dos outros 11 cenários deste `sandbox`),
`dynamicsModel: ( JSBSimModel )`, decidindo via o mesmo agente de produção
(`( FlightAgentTC )`) com a árvore de **um nó só**, `flight_tree_nav.xml` (a mesma de
`A4-6DOF`/`full-systems-nav`: `( Navigate )`, sem patrulha/evasão/RTB por baixo).

**A linha reta é só um steerpoint, bem longe.** Em vez de uma rota de vários pontos, `( Route )`
tem **um único** `( Steerpoint )`, a ~288 km de distância, na MESMA latitude da largada
(`autoSequence: false` — nunca sequencia para outro ponto). Como a marcação para um ponto
diretamente à frente, na mesma latitude, não muda enquanto se voa em linha reta na direção
dele, `( Navigate )` mantém rumo 090 (exatamente leste) do primeiro ao último frame — sem
precisar de dezenas de waypoints para "voar reto".

**A altitude de 300 ft AGL não é uma rota pré-calculada — é o piso anti-CFIT em ação o tempo
todo.** `bt_nodes::NavigateAction::tick()` sempre passa a altitude comandada por
`BtBehavior::clampAltitudeToTerrain()` (o mesmo piso que `Patrol`/`ReturnToBase`/`SupportAlert`
já usam), e `domain::clampToTerrain()` **nunca abaixa, só levanta**. O steerpoint comanda uma
altitude absurdamente baixa (`-2000 m`) de propósito — o clamp sempre vence, então o valor que
chega no `Autopilot`, todo frame de fundo, é **exatamente** `elevação real do terreno sob a
aeronave (lida ao vivo por `Player::updateElevation()` → `MultiTileTerrain::getElevation()`) +
300 ft (terrainClearance:, ajustado explicitamente em `( BtBehavior )`, default é 500 m)`. Voar
"acompanhando o terreno" é literalmente o piso anti-CFIT reagindo à leitura de elevação real, em
tempo real — não um roteiro com dezenas de pontos de altitude fixados a mão.

`crashOverride: true` é deliberado: voando a só 300 ft de folga contra relevo real (que em
alguns trechos sobe ~270 m por km), um instante de atraso do autopilot atrás do piso pode gerar
um AGL negativo transitório — sem isso, `CRASH_EVENT` congelaria a aeronave no meio do
corredor. O objetivo aqui é completar o corredor observando a telemetria real, não provar que o
A-4 nunca raspa o chão.

## O corredor real: três tiles vendorizados, uma fronteira cruzada de verdade

Latitude fixa **-22.7** (não -22.25, como `A4-6DOF`) — escolhida por **amostragem real do
próprio dado SRTM**, não arbitrária: é a faixa mais suave entre várias latitudes testadas nesta
mesma região (a Serra do Mar tem escarpamentos de mais de 800 m em 3 km em outras latitudes do
mesmo corredor). Largada em `lon -43.9` (tile `S23W044`, elevação real ~428 m), destino em
`lon -41.1` (tile `S23W042`), atravessando de verdade a fronteira real `S23W044`/`S23W043`
(`lon -43.0`, ~92 km da largada) e depois `S23W043`/`S23W042` (`lon -42.0`, ~195 km da largada).
Os três tiles são reais e já vendorizados neste repositório (`shared/data/terrain/srtm/
README.md`) — nenhum precisa de `scripts/fetch_srtm.sh`.

## ARMADILHA CONFIRMADA RODANDO — bug do MIXR nativo, não deste cenário

Construindo e rodando este cenário (não hipotético — `-deterministic 60000`, com telemetria e
dump conferidos linha a linha), apareceu um bug real na decodificação de elevação SRTM, **fora**
do escopo do que este cenário deveria testar (multi-tile), mas exposto por ele.

**O que é**: `terrain::SrtmHgtFile::readValue()`
(`contexts/src/mixr/src/terrain/srtm/SrtmHgtFile.cpp`) decodifica cada amostra de 16 bits como
**sinal-e-magnitude** (testa o bit alto do byte alto, zera esse bit, nega a magnitude) — mas o
formato `.hgt` real, documentado no cabeçalho do PRÓPRIO arquivo, é **complemento de 2**, o int16
big-endian padrão. As duas codificações só divergem quando a elevação real é **negativa**:
qualquer elevação real negativa `R` (magnitude `1..~32000`) decodifica como `magnitude - 32768`
em vez de `R` — uma elevação real de `-70 m`, por exemplo, sai como `-32698`.

**Onde isso aparece neste corredor** (medido, não suposto): duas vezes. Primeiro, breve, dentro
do próprio `S23W044` (~81-86 km da largada — um pequeno vale/riacho real, elevação -3/-4 m, dois
trechos de ~1,9 km e ~5 km). Depois, sustentado: ao entrar na baixada costeira de `S23W042`
(~196 km da largada em diante), onde a elevação real fica negativa a maior parte do tempo —
dali até o destino, o bug fica ativo quase o corredor inteiro.

**Por que isso NÃO é perigoso para a aeronave, só para a telemetria** — confirmado lendo
`domain::terrainFloorM()` (`models/players/air/A-4/src/domain/TerrainFloor.cpp`):
`max(minSafeAltitude, ground.elevationM + clearanceM)`. Com `ground.elevationM` incorretamente em
~-32700, o termo "terreno + folga" fica muito mais negativo que o piso absoluto — quem vence o
`max()` é sempre o piso absoluto, nunca o valor bugado. **Atualizado após a correção do piso
absoluto (ver a seção logo acima)**: este cenário declara `minSafeAltitude: ( Meters 0 )`, não
mais os 200 m fixos de antes — medido no dump, durante esses trechos `alt=` agora fica travado
perto de **0 m MSL** (não mais ~200,06 m) enquanto `elev=`/`agl=` continuam mostrando números sem
sentido (`elev=-32767`, `agl=32787`...) — a aeronave voa nivelada e segura perto do nível do mar;
só o **campo relatado** de elevação/AGL (e portanto o acompanhamento fino do relevo) fica errado
enquanto isso dura. Confirmado nas 154 amostras onde o piso domina numa corrida completa: **todas**
caem exatamente nesses dois trechos (`terrainElevM < -1000`), nenhuma é terreno real genuinamente
baixo sendo mascarado por engano.

**O que isso significa para quem lê `data/messages/mission_*.jsonl`**: os campos `terrainElevM`/
`altAglM` são confiáveis do início do corredor até ~196 km — é exatamente esse trecho, cruzando
a fronteira real `S23W044`/`S23W043`, que demonstra a validação de carregamento multi-tile que
este cenário existe para provar. Dali em diante os dois campos refletem o bug de decodificação
acima, um defeito **diferente**, já isolado e confirmado — não é motivo para reabrir a
investigação do multi-tile.

Este é um bug do **MIXR nativo, vendorizado** (`contexts/src/mixr/`) — dependência binária deste
projeto, nunca objeto de desenvolvimento aqui (ver o topo do `CLAUDE.md` da raiz). Não foi (nem
deveria ser) corrigido neste repositório; fica documentado aqui, e não redescoberto, pela mesma
disciplina que rege todas as outras dezenas de armadilhas já confirmadas no `CLAUDE.md`.

## O que não há, de propósito (mantém o cenário "muito simples")

Sem sensor/radar/IFF/`( StoresMgr )`/assinatura (`FlightState.cpp` degrada tudo isso com graça
quando ausente), sem segundo player, sem `networks:`, sem bandit. Só `DynamicsModel` + `Pilot`
(`Autopilot`) + `Navigation` (`Route` de **um** steerpoint) + o agente UBF de produção.

## Como verificar

```bash
# 1. lint estrutural
python3 src/ui/scripts/edl_lint.py sandbox/A4-6DOF-NBA/configs/scenario_a4_6dof_nba.edl.in

# 2. o parser de verdade do MIXR
sed 's/@NUM_TC_THREADS@/2/; s/@NUM_BG_THREADS@/2/; s/@RUN_ID@/x/g' \
   sandbox/A4-6DOF-NBA/configs/scenario_a4_6dof_nba.edl.in > /tmp/A4-6DOF-NBA.edl
./build/app/src/edlcheck /tmp/A4-6DOF-NBA.edl

# 3. roda de verdade, headless, e confere a telemetria de terreno
./build/app/src/app -folder ./sandbox -scenario A4-6DOF-NBA -deterministic 60000 > /tmp/nba.log
grep '^frame=' /tmp/nba.log | tail -5
cat sandbox/A4-6DOF-NBA/data/messages/mission_*.jsonl | tail -5   # terrainElevM/altAglM
```

`bt=NAV` em toda linha do dump é o esperado (única folha da árvore). `dec=` avança na mesma
taxa que `frame=` entre dumps consecutivos, como em qualquer cenário deste repositório.
`altAglM` na telemetria deve oscilar perto de 300 ft (~91 m, tipicamente 70-170 m) na maior parte
do corredor — não mais travado num platô constante (ver a seção "ARMADILHA CONFIRMADA E
CORRIGIDA" acima); só nos dois trechos do bug SRTM (`terrainElevM` na casa de `-327xx`) é que
`altMslM` fica perto de 0 m.
