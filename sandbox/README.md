# sandbox/ — cenários soltos de experimentação

Ao contrário de `src/poc/`, onde cada pasta isola **uma** variável de integração (ver o README da
raiz, seção "Como o projeto se organiza"), `sandbox/` é o espaço de **experimentação livre**:
cenários mais completos ("player máximo", frotas grandes, rotas reais), usados para exercitar
modelos e mecanismos do framework fora do recorte estreito de uma poc. Nenhum código mora aqui —
cada pasta é só `configs/` (o `.edl.in` do cenário, mais árvore/script/`.onnx` quando o cenário
precisa) e um `README.md`.

Todos rodam pelo mesmo runner único do repositório:

```bash
./build/app/src/app -folder ./sandbox -scenario <pasta>                 # tempo real
./build/app/src/app -folder ./sandbox -scenario <pasta> -deterministic <N>  # headless
make run-app                                                            # atalho p/ '-folder ./sandbox'
                                                                         # (sem -scenario: abre a tela de selecao)
```

Os 11 cenários abaixo **compartilham a porta Tacview 1234** (confirmado nos 11 `.edl.in`) — de
propósito, já que nenhum roda ao mesmo tempo que outro.

**`sandbox/` é gitignored por padrão.** `sandbox/.gitignore` ignora qualquer subpasta nova
(`/*/`) e reabre, por nome, só as 11 que este repositório já traz como exemplo (`!/A4-6DOF/`
etc.) — criar uma pasta nova aqui para testar algo fica automaticamente **fora do git**, sem
precisar tocar em `.gitignore` nenhum. Para transformar uma experiência sua num exemplo
versionado, acrescente `!/<nome-da-pasta>/` na lista de `sandbox/.gitignore` e rode `git add
sandbox/<nome-da-pasta>/`. Independente disso, o **conteúdo gerado em runtime** de qualquer pasta
(`data/recordings/*.acmi`, `data/logs/*.log*`, `data/messages/*.jsonl`) é sempre ignorado pelo
`.gitignore` da raiz — só o `.gitkeep` de cada `data/` é versionado, mesmo padrão de `src/poc/`.

## Índice dos cenários

| pasta | o que demonstra | modelo(s) | derivado de |
|---|---|---|---|
| [A4-6DOF](A4-6DOF/README.md) | **Base da família `A4-*DOF`.** Oito A-4 empilhados (1.000 ft de separação) voando uma figura-de-oito fechada de 20 steerpoints, dinâmica `JSBSimModel` (6-DOF), decisão nativa (`( Navigate )`) | A-4 | — é a referência; nasceu do antigo `full-systems-nav`, hoje fixture em `tests/fixtures/` |
| [A4-4DOF](A4-4DOF/README.md) | Mesma frota/rota de A4-6DOF; só `dynamicsModel:` muda para `LaeroModel` (4-DOF, contagem documentada pelo próprio framework) | A-4 | **A4-6DOF**, linha a linha — o diff é só os 8 blocos `dynamicsModel:` (comando verificável no README) |
| [A4-3DOF](A4-3DOF/README.md) | Mesma frota/rota de A4-6DOF; `dynamicsModel:` vira `RacModel` (cinemático; "3DOF" é rótulo informal, sem contagem de DOF no framework) | A-4 | **A4-6DOF**, linha a linha — mesmo diff restrito ao `dynamicsModel:` |
| [A4-4DOF-PY](A4-4DOF-PY/README.md) | Um único A-4, `LaeroModel`, decisão por folhas em Python (`( PyDecide )`) — nunca completa a rota: o contrato de 28 floats não carrega dado de steerpoint | A-4 | fork antigo de A4-4DOF, de **antes** dele acompanhar A4-6DOF — hoje fora de sincronia (frota de 1 e rota de 4 pontos, não as de A4-6DOF) |
| [A4-4DOF-ONNX](A4-4DOF-ONNX/README.md) | Um único A-4, `LaeroModel`, decisão por rede neural (`( OnnxPolicy )`, 6.211 parâmetros) — mesma limitação: nunca completa a rota | A-4 | mesma linhagem de A4-4DOF-PY, também fora de sincronia |
| [A4-6DOF-RANDOM](A4-6DOF-RANDOM/README.md) | A4-6DOF + um nó `( SlowRoll )`: *tonneaux* em instantes aleatórios, porém reprodutíveis (`libs/xrandom`) | A-4 | **A4-6DOF** — mesma frota/rota; só a árvore (`flight_tree_random.xml`) e slots de acrobacia mudam |
| [A4-6DOF-MISSILE](A4-6DOF-MISSILE/README.md) | Dois A-4 (atirador + alvo): detecção por radar → envelope de disparo → guiagem proporcional → detonação de um míssil cinemático | A-4 + `missile` | independente — primeiro de dois exercícios da mesma série (o segundo é AAA-A4-6DOF) |
| [AAA-A4-6DOF](AAA-A4-6DOF/README.md) | Uma antiaérea (`( AaaSite )`) detecta e dispara o mesmo míssil contra um A-4, que evade via RWR **passivo** com atraso de reação estocástico | A-4 + `AAA` + `missile` | segunda da mesma série que A4-6DOF-MISSILE — reaproveita o `( GuidedMissile )` |
| [C-130-6DOF](C-130-6DOF/README.md) | Reaproveita a **geometria de rota** de A4-6DOF (mesma figura-de-oito de 20 pontos) pilotada por um C-130, não pelo A-4 | `C-130` | reaproveita a **rota** de A4-6DOF (não o `.edl` inteiro — frota e modelo mudam: 1 C-130, não 8 A-4) |
| [C-130_paratrooper-6DOF](C-130_paratrooper-6DOF/README.md) | Integração real: um C-130 libera **30 paraquedistas de verdade** (`( Paratrooper )`, não um placeholder) em sequência de 1,5 s, numa rota própria mais simples | `C-130` + `paratrooper` | independente — rota nova de 3 steerpoints, não a figura-de-oito |
| [Navstar-3-constellation](Navstar-3-constellation/README.md) | Quatro satélites `Navstar-3`, mesma classe, quatro planos orbitais distintos (`raan` diferente em cada um) | `Navstar-3` (`models/players/space/Navstar-3`) | independente — único cenário orbital deste `sandbox/`, não pilota aeronave nenhuma |

## Por onde começar

Um novato seguindo ordem alfabética cairia em `A4-3DOF` primeiro — mas é a variante, não a base.
Ordem sugerida:

1. **[A4-6DOF](A4-6DOF/README.md)** — comece aqui. É a referência da família `A4-*DOF`: entenda a
   frota de oito, a construção da rota em figura-de-oito e por que `JSBSimModel` (6-DOF) é o único
   dos três `dynamicsModel:` da família com contagem de graus de liberdade documentada de verdade
   pelo framework.
2. **[A4-4DOF](A4-4DOF/README.md)** e **[A4-3DOF](A4-3DOF/README.md)** — mesma frota/rota de
   A4-6DOF, só o `dynamicsModel:` muda. Leia os dois para comparar o que cada modelo de dinâmica
   preserva do `( Autopilot )` e o que fica inerte.
3. **[A4-4DOF-PY](A4-4DOF-PY/README.md)** e **[A4-4DOF-ONNX](A4-4DOF-ONNX/README.md)** — variantes
   mais antigas, de um único player, trocando **quem decide** (script Python / rede ONNX) em vez do
   nó nativo `( Navigate )`. Note a ressalva de proveniência na tabela acima: ficaram fora de
   sincronia com a frota/rota que A4-6DOF tem hoje.
4. **[A4-6DOF-RANDOM](A4-6DOF-RANDOM/README.md)** — mesma base, acrescenta uma acrobacia aleatória
   reprodutível por cima da navegação nativa.
5. **[A4-6DOF-MISSILE](A4-6DOF-MISSILE/README.md)** → **[AAA-A4-6DOF](AAA-A4-6DOF/README.md)** —
   série temática de exercícios: primeiro um A-4 dispara um míssil contra outro por radar ativo;
   depois uma antiaérea dispara o mesmo míssil contra um A-4, que evade por RWR passivo.
6. **[C-130-6DOF](C-130-6DOF/README.md)** → **[C-130_paratrooper-6DOF](C-130_paratrooper-6DOF/README.md)**
   — troca de modelo sobre a mesma geometria de rota da família A4, depois a integração completa
   (C-130 largando paraquedistas de verdade).
7. **[Navstar-3-constellation](Navstar-3-constellation/README.md)** — eixo totalmente diferente dos
   dez anteriores: órbita, não voo atmosférico.

## Vocabulário extra do Steerpoint: sca, magvar, pta

Os `Steerpoint` das rotas de A4-6DOF/A4-4DOF/A4-3DOF (a figura-de-oito de 20 pontos) e de
A4-4DOF-PY/A4-4DOF-ONNX (a rota mais antiga de 4 pontos, estruturalmente presente mas nunca
alcançada por essas duas variantes — ver a limitação de cada README) carregam três slots nativos
que `mixr::models::Steerpoint` já aceita (`Steerpoint.hpp`) e que rotas mais antigas do repositório
não usavam:

- **`sca`** (*Safe Clearance Altitude*, pés)
- **`magvar`** (declinação magnética, graus)
- **`pta`** (*Planned Time of Arrival*, segundos)

Os três são recalculados **todo frame** por `Steerpoint::compute()` — mas **nenhum consumidor
deste repositório** (`FlightState.cpp`/`NavigateAction.cpp`/`libs/xmsg`) lê o resultado
(`isWarnSCA()`/`getMagBrgDeg()`/`getELT()`). A adição é por completude de vocabulário EDL, não por
efeito medido no voo.

`stptType` cobre **6 dos 7** valores nativos (`Steerpoint.hpp`: `enum StptType { DEST, MARK, FIX,
OAP, IP, TGT, TGT_GRP }`), repetidos ao longo dos pontos. O sétimo, **`TGT_GRP`, não é alcançável
por EDL**: existe no enum, mas `Steerpoint::setSlotStptType()` (`Steerpoint.cpp:325-340`) só
reconhece as seis strings acima e recusa qualquer outra com *"invalid steerpoint type"* — escrever
`TGT_GRP` no `.edl` derruba o slot, não seleciona o tipo.

## Por que o combustível aparece zerado no dump

O dump de `-deterministic` mostra `fuel=0.000000000` (e, sob `LaeroModel`, também
`mach=0.000000000`) em qualquer cenário deste `sandbox/` que **não** use `( JSBSimModel )` como
`dynamicsModel:` — ou seja, A4-4DOF, A4-3DOF, A4-4DOF-PY e A4-4DOF-ONNX. **Não é um defeito.**

O campo `fuel=` (`app/src/app/DeterministicDump.cpp`) lê `AirVehicle::getFuelWt()` direto, que só é
populado por um `DynamicsModel` que simula tanque de combustível de verdade (`JSBSimModel`, via os
dados de `data/jsbsim/`); `LaeroModel`/`RacModel` não modelam isso, então o valor cru fica em `0`.
`mach=` segue o mesmo padrão (campo específico do FDM do JSBSim, sem equivalente nos outros dois
`dynamicsModel:`).

**Isso não afeta a decisão do UBF.** `domain::WorldView::fuelFraction` — o campo que a condição
`FuelLow` de fato consulta — é calculado em `FlightState.cpp` com uma guarda explícita:
`(fuelMax > 0.0) ? (getFuelWt()/fuelMax) : 1.0`. Sem tanque simulado ele cai para `1.0` (tanque
cheio, nunca baixo), não `0.0` — `FuelLow` não dispara por engano em nenhum dos quatro cenários
acima.

## edlcheck e o token NUM_TC_THREADS

`edlcheck` (`app/src/edlcheck_main.cpp`) valida um `.edl` sem levantar `Station`/frota/terreno —
mas recusa qualquer `.edl.in` **cru** deste `sandbox/` com `error while setting slot name:
numTcThreads`. O token `@NUM_TC_THREADS@` só é expandido em runtime, por
`app::generateScenario()` (`ScenarioTemplate`) — nunca pelo `edlcheck` direto, que não passa por
esse passo de template.

Para validar via `edlcheck`, resolva o(s) token(s) manualmente antes:

```bash
sed 's/@NUM_TC_THREADS@/2/; s/@RUN_ID@/x/g' sandbox/<pasta>/configs/<arquivo>.edl.in > /tmp/<pasta>.edl
./build/app/src/edlcheck /tmp/<pasta>.edl
```

`@RUN_ID@` (quando presente — ex.: em nomes de arquivo como `mission_@RUN_ID@.acmi`) é outro token
de template; o `s/@RUN_ID@/x/g` acima resolve os dois de uma vez.

## Como verificar qualquer cenário deste sandbox

A receita abaixo é a mesma nos 11 cenários — troque `<pasta>` e `<arquivo>` pelo nome da subpasta e
pelo `.edl.in` que ela usa (ver a tabela do índice, acima). Cada README individual documenta só o
que diverge disso: rótulos `bt=` esperados, scripts extras de determinismo/semente, etc.

```bash
# 1. lint estrutural (fabrica/slot desconhecido, ASCII, referencia solta)
python3 src/ui/scripts/edl_lint.py sandbox/<pasta>/configs/<arquivo>.edl.in

# 2. o parser de verdade do MIXR -- resolva @NUM_TC_THREADS@/@RUN_ID@ antes (ver acima)
sed 's/@NUM_TC_THREADS@/2/; s/@RUN_ID@/x/g' sandbox/<pasta>/configs/<arquivo>.edl.in > /tmp/<pasta>.edl
./build/app/src/edlcheck /tmp/<pasta>.edl

# 3. roda de verdade, headless, e confere os rotulos bt= no dump
./build/app/src/app -folder ./sandbox -scenario <pasta> -deterministic 600 > /tmp/<pasta>.log 2>&1
grep -o 'bt=[A-Za-z_-]*' /tmp/<pasta>.log | sort -u

# 4. determinismo com 1, 2 e 4 threads T/C -- dumps devem sair byte-identicos,
#    mais uma repeticao de 4 threads
./tests/determinism/check_determinism.sh ./build/app/src/app <pasta> 2000 '' \
    sandbox/<pasta>/configs/<arquivo>.edl.in
```

Nenhum cenário deste `sandbox/` entra em `tests/meson.build` (a suíte automatizada do core) — a
verificação é sempre manual, pelos passos acima.
