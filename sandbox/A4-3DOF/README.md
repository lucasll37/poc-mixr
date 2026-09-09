# A4-3DOF — oito players máximos empilhados, dinâmica RacModel (cinemática), voando uma figura-de-oito

Um dos **cinco** cenários da família `A4-*DOF` deste `sandbox/` — ver
`sandbox/A4-6DOF/README.md` para a tabela completa da família e a porta
Tacview compartilhada (**1234**, de propósito). Esta variante troca **só** o
`dynamicsModel:` em relação a `A4-6DOF`.

```bash
./build/app/src/app -folder ./sandbox -scenario A4-3DOF        # tempo real, Tacview 1234
./build/app/src/app -folder ./sandbox -scenario A4-3DOF -deterministic 600
```

## "Idêntico a A4-6DOF exceto o `dynamicsModel:`" — e como conferir

O `.edl.in` desta pasta é o de `A4-6DOF` **linha a linha**: a frota de **oito**
aeronaves idênticas (`a4_1`..`a4_8`, empilhadas com 1.000 ft de separação
vertical), a **figura-de-oito de 20 steerpoints** com o perfil de altitude
entre 4.000 e 15.000 ft (11.000 a 22.000 ft na mais alta), os 53 componentes
de cada player, `pilot:`/`Autopilot`, `agent:`/árvore, terreno e `provides:`.
Fora do bloco `dynamicsModel:` mudam apenas os caminhos/rótulos que carregam o
nome desta pasta (`data/`, `callsign`, `eventName`) e o cabeçalho de
comentário. Não é uma promessa — é verificável:

```bash
diff <(sed 's/A4-3DOF/A4-6DOF/g' sandbox/A4-3DOF/configs/scenario_a4_3dof.edl.in | grep -v '^\s*//') \
     <(grep -v '^\s*//' sandbox/A4-6DOF/configs/scenario_a4_6dof.edl.in)
```

Medido: o diff acima é **exatamente** os 8 blocos `dynamicsModel:`, nada mais.

> **Isto é uma mudança recente.** Antes, esta pasta ainda tinha **um** player
> `a4` e a rota original de 4 steerpoints, enquanto `A4-6DOF` já tinha
> evoluído para a frota de oito e a figura-de-oito — o README daqui afirmava
> "byte a byte idêntico" sem que fosse verdade. Agora é.

## "3DOF" é um rótulo informal — leia isto antes de "corrigir" o nome

`RacModel` ("Robot Aircraft") **não tem doc string de graus de liberdade
nenhuma** no próprio header
(`contexts/src/mixr/include/mixr/models/dynamics/RacModel.hpp`: só "Very
simple dynamics model") — ao contrário de `LaeroModel` (`A4-4DOF`), que É
documentado como "4 degree of freedom" pelo próprio framework. `RacModel` é
melhor descrito como um modelo **cinemático**: rumo/altitude/velocidade são
cada um um canal comandado (`cmdHeading`/`cmdAltitude`/`cmdSpeed`) que
converge por um limitador de taxa simples, **sem estado de dinâmica
rotacional nenhum** (confirmado lendo `RacModel.cpp::updateRAC()` — não há
equação de momento resolvendo taxa de rolagem/arfagem/guinada a partir de
forças). "3DOF" aqui é o número de canais comandados independentes (rumo,
altitude, velocidade), não uma contagem formal do framework — decisão tomada
com o usuário ao criar este cenário, preferida a "1DOF" (que sugeriria quase
nenhuma liberdade, impreciso na outra direção).

## O bloco `dynamicsModel:`

```
dynamicsModel: ( RacModel
   minSpeed:    100.0
   speedMaxG:   250.0
   maxg:        4.0
   maxAccel:    10.0
   cmdAltitude: ( Feet 11000 )      // = o initAlt DESTA aeronave
   cmdHeading:  ( Degrees 54 )
   cmdSpeed:    262.0
)
```

É o **único** bloco que varia entre as oito aeronaves: `cmdAltitude` carrega
o `initAlt` de cada uma (11.000 ft na `a4_1` … 18.000 ft na `a4_8`), pelo
mesmo motivo que `initAlt` varia. Os outros seis valores são iguais nas oito.

Justificativa de cada número (sem precedente de uso de `RacModel` neste
repositório — nenhum `.edl` de produção o usa):

- `cmdAltitude`/`cmdHeading`/`cmdSpeed` = os mesmos `initAlt`/`initHeading`/
  `initVelocityKts` do player — estado inicial coerente com a pose real de
  partida; o `Autopilot` sobrescreve isso a cada frame de qualquer forma
  (`headingHoldMode`/`altitudeHoldMode`/`velocityHoldMode` são todos `true`).
  `cmdAltitude` é `base::Distance` (aceita `( Feet ... )`) e `cmdSpeed` é em
  **nós** de verdade — `RacModel.cpp:280` converte por `NM2M/3600` antes de
  comparar.
- `maxg: 4.0` / `maxAccel: 10.0` = os próprios **defaults compilados** da
  classe (`RacModel.hpp`: `gMax{4.0}`, `maxAccel{10.0}`) — usa-se o default
  do autor em vez de inventar número novo.
- `minSpeed: 100.0` / `speedMaxG: 250.0` = **em m/s** (ver a armadilha 1
  abaixo), recalibrados para a faixa de ~135 m/s (262 kt) que os steerpoints
  desta rota comandam. `updateRAC()` interpola `gmax` linearmente de 1 G em
  `V == minSpeed` até `maxg` em `V == speedMaxG`; com `minSpeed` **acima** da
  velocidade de voo a rampa produz `gmax < 1` e a curva degrada — era o que
  aconteceria mantendo os `150.0`/`400.0` da versão anterior deste cenário,
  calibrados para os 350 kt da rota antiga. `speedMaxG: 250.0` é o default
  compilado da classe.

## Duas armadilhas do framework, não desta mudança

Documentadas aqui para ninguém "consertar" os números acima sem saber o motivo:

1. **Unidade inconsistente no `RacModel` nativo**: `setSlotMinSpeed`/
   `setSlotSpeedMaxG` (`RacModel.cpp:304-322`) guardam o valor **cru**, e
   `updateRAC()` o compara direto contra `Player::getTotalVelocity()`, que é
   **m/s** — apesar do comentário do slot dizer "(kts)" e o do membro
   `vpMaxG` dizer "(g's)". Confirmado lendo o fonte. Inconsistência nativa do
   MIXR, fora do escopo desta poc corrigir (o projeto trata o MIXR como
   dependência binária).
2. **Parâmetros secundários do `Autopilot` ficam inertes sob `RacModel`**:
   `maxRateOfTurnDps`/`maxBankAngle`/`maxPitchAngle`/`maxClimbRateMps`
   (copiados sem mudança no `pilot:`, herdados de `A4-6DOF`) chegam aos três
   `setCommanded*()` do `RacModel` (`:129`, `:152`, `:175`) como parâmetros
   **sem nome** e são descartados — ao contrário do `LaeroModel` (`A4-4DOF`),
   que os usa de verdade. Quem limita a manobra é a rampa de G acima; o teto
   de razão de subida é *hard-coded* no fonte (`:207`).

## Consequência herdada: o `airspeed:` calibrado dos steerpoints não vale aqui

Cada steerpoint de `A4-6DOF` carrega uma velocidade **calibrada** recalculada
por aeronave, porque o autopiloto do airframe JSBSim fecha a malha contra
`velocities/vc-kts` — é o que segura a pilha de oito junta. `RacModel` trata
o valor comandado como velocidade **verdadeira** (`RacModel.cpp:280`), então
essa compensação por altitude não tem para quem valer: as oito comandam
verdadeiras ligeiramente diferentes entre si e a pilha se estica ao longo da
rota. **Os números foram mantidos idênticos de propósito** — a única
diferença pedida para este cenário é o `dynamicsModel:`. Para colar a
formação sob este modelo, os vinte `airspeed:` teriam de voltar a ser o mesmo
valor (262) nas oito; é mudança de cenário, não desta variante.

Efeito medido numa volta (`-deterministic 118000`): a `a4_1` voa entre 108 e
133 m/s (contra os 135 m/s nominais do `A4-6DOF`) e a volta não fecha dentro
dos 2.360 s simulados — para ~12 km do ponto de partida.

## Medido rodando

`-deterministic 600`: `bt=NAV` em 100% das linhas, as **oito** aeronaves
presentes no dump, zero erro de parse, zero `was not found!`.

`-deterministic 118000` (uma volta, ~2.360 s simulados): perfil de altitude
cumprido de ponta a ponta — `a4_1` entre **3.859 e 15.141 ft** e `a4_8` entre
**10.858 e 22.141 ft** (o comando é 4.000–15.000 / 11.000–22.000; o
excedente é o *overshoot* da malha, não deriva) — e **AGL mínimo de 1.056 m**
sobre toda a frota: ninguém chega perto do terreno. A trajetória cobre os
dois lobos do oito (~71 km em norte, ~82 km em leste).

Determinismo: `check_determinism.sh` passa com **1, 2 e 4 threads** T/C,
dumps byte-idênticos, mais a repetição de 4 threads.

`fuel=0.000000000` no dump é esperado sob `RacModel` (mesma explicação de
`A4-4DOF/README.md` — `getFuelWt()` cru fica em 0 sem tanque simulado, mas
`domain::WorldView::fuelFraction` cai para `1.0`, não `0.0`, então `FuelLow`
nunca dispara por engano).

## Vocabulário extra do Steerpoint: `sca`/`magvar`/`pta`

Ver `sandbox/A4-6DOF/README.md` — os 20 `Steerpoint` desta rota (idêntica à
de `A4-6DOF`) carregam os mesmos três slots nativos ociosos, recalculados
todo frame por `Steerpoint::compute()` mas sem consumidor neste repositório.

## Verificação manual

```bash
python3 src/ui/scripts/edl_lint.py sandbox/A4-3DOF/configs/scenario_a4_3dof.edl.in

# edlcheck recusa o .edl.in cru por causa de @NUM_TC_THREADS@ -- resolva o token antes
sed 's/@NUM_TC_THREADS@/2/; s/@RUN_ID@/x/g' sandbox/A4-3DOF/configs/scenario_a4_3dof.edl.in > /tmp/a4-3.edl
./build/app/src/edlcheck /tmp/a4-3.edl

./build/app/src/app -folder ./sandbox -scenario A4-3DOF -deterministic 600 > /tmp/a4-3dof.log 2>&1
grep -o 'bt=[A-Za-z_-]*' /tmp/a4-3dof.log | sort -u    # esperado: so bt=NAV
grep -o 'player=a4_[0-9]' /tmp/a4-3dof.log | sort -u   # esperado: as 8 aeronaves

./tests/determinism/check_determinism.sh ./build/app/src/app A4-3DOF 2000 '' \
    sandbox/A4-3DOF/configs/scenario_a4_3dof.edl.in
```

## O que herda sem mudança

Tudo de `A4-6DOF` exceto o `dynamicsModel:` — os 53 componentes em cada uma
das oito aeronaves, a rota em figura-de-oito de 20 steerpoints com as três
`Action` (decoy/SAR/troca de camuflagem) disparando a cada volta,
`pilot:`/`Autopilot` (embora parte dele fique inerte, ver acima),
`agent:`/`flight_tree_nav.xml`, terreno SRTM compartilhado, `provides:`.
