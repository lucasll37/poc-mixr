# Arquitetura do modelo `Navstar-3`

## O que este modelo e

Um satelite de navegacao (familia GPS/NAVSTAR) orbitando a Terra. Primeiro modelo deste
repositorio na categoria `others` (`models/others/`, antes so' um `.gitkeep`) -- os cinco
pedacos que `make new-model` cobra (`tests/`, `docs/`, `README.md`, `CHANGELOG.md`, `Makefile`)
sao os mesmos de qualquer modelo `player`, so' a subpasta muda.

Elementos orbitais default sao valores REPRESENTATIVOS de uma orbita GPS/MEO moderna (~20.180 km
de altitude, ~55 graus de inclinacao, periodo ~12h) -- NAO e reivindicada precisao historica para
o Navstar-3 real (SVN-3, GPS Block I, lancado em 1978 e desativado ha decadas, sem efemeride
precisa preservada e util para reproduzir aqui). Os quatro elementos (`altitude`/`inclination`/
`raan`/`argLat0`) sao slots do `( Navstar3BtBehavior )` -- qualquer outra orbita circular e' so'
mudanca de EDL, sem recompilar.

## O MIXR nao tem mecanica orbital nativa nenhuma -- investigado antes de escrever uma linha

Confirmado lendo o fonte vendorizado (`contexts/src/mixr/`), nao suposto pelo header:

- `mixr::models::SpaceVehicle` (`include/mixr/models/player/space/SpaceVehicle.hpp`) e' uma
  casca fina sobre `Player` -- `EMPTY_SLOTTABLE`, zero slots proprios. `getMajorType()` devolve
  `SPACE_VEHICLE` (bit `0x80` de `Player::MajorType`).
- `mixr::models::SpaceDynamicsModel` e' um STUB TOTAL -- os quatro metodos que ele acrescenta
  sobre `DynamicsModel` sempre `return false` sem tocar em estado nenhum, e `DynamicsModel::
  dynamics()` (a base) e' uma funcao com corpo VAZIO. Nao ha integracao de aceleracao
  gravitacional, nao ha propagador, nao ha nada.
- `MannedSpaceVehicle`/`UnmannedSpaceVehicle`/`BoosterSpaceVehicle` nao acrescentam fisica
  alguma (confirmado no `.cpp` de cada uma, nao so' suposto pelo header).
- `mixr::base` nao tem Kepler, RAAN, anomalia, ECI/J2000 ou `Ephemeris` em lugar nenhum (busca
  exaustiva por `grep`). O que existe sao conversoes ESTATICAS de coordenadas
  (`base::nav::convertGeod2Ecef()`/`convertEcef2Geod()`, `base::nav::ERADM`) -- uteis, mas sem
  nocao nenhuma de tempo/dinamica.

**PRIOR ART.** Uma poc quase identica ja existiu neste repositorio e foi removida na
reestruturacao geral que apagou toda a progressao numerada antiga (`poc/10-satellite-
constellation`, commit `87c6b15` -> removido em `52a71e4` -- recuperavel via `git show
87c6b15:poc/10-satellite-constellation/{include/orbit.hpp,src/orbit.cpp}`). O comentario do
proprio autor daquela poc ja concluia o mesmo: *"MIXR nao tem nenhum propagador orbital nativo --
SpaceVehicle/SpaceDynamicsModel sao stubs sem fisica nenhuma"*. As formulas de orbita circular e
o padrao `setGeocPosition(ecef, slaved=true)` usados aqui adaptam aquele design (arquitetura
pre-plugin, nao copiada literalmente) para a camada domain/bt/ubf/xnative de hoje.

## Como o satelite se move -- sem `DynamicsModel` nenhum

`navstar3: ( SpaceVehicle ... )` NAO declara `dynamicsModel:`. Quem move o player e'
`domain::CircularOrbit`/`domain::EclipseGeometry` (puros, sem MIXR), aplicados via
`Player::setGeocPosition(ecefPos, /*slaved=*/true)` dentro de `ubf::Navstar3Action::execute()`.

`slaved=true` grava `posSlaved=true; altSlaved=true` dentro de `setGeocPosition()`
(`Player.cpp:1958-1963`) -- e isso e' o que desliga o integrador cinematico NATIVO de
`Player::positionUpdate()` para este player (`enabled = vp>0 && dt!=0 && (!pfrz||!afrz)` avalia
falso). Sem isso, o integrador nativo (que so' anda em linha reta na velocidade corrente)
disputaria a posicao com o calculo orbital a cada frame. `setGeocVelocity(ecefVel)` (sem
parametro `slaved` -- essa sobrecarga nao tem um, confirmado no header) so' alimenta os campos de
velocidade/leitura (`vp`, `gndSpd`, ...), usados por telemetria/Tacview -- nao ha integrador de
velocidade competindo.

Terreno/AGL nao atrapalham, por construcao: `Player::positionUpdate()` exclui `SPACE_VEHICLE` da
mascara de clamp ao solo (so' se aplica a `GROUND_VEHICLE|SHIP|BUILDING|LIFE_FORM`), e o teste de
`CRASH_EVENT` por AGL negativa nunca dispara a ~20.180 km de altitude, com ou sem `terrain:`
declarado. A UNICA coisa a evitar e' declarar `gamingAreaRange:` finito no `WorldModel` --
`setGeocPosition()` marca `posVecValid=false` quando a distancia local excede esse raio, e uma
orbita MEO facilmente excede qualquer raio de "area de jogo" tipico de cenario militar. O cenario
de producao deste modelo (`src/poc/navstar3-orbit`) simplesmente nao declara esse slot.

## O propagador orbital -- `domain::CircularOrbit`

Duas funcoes puras (`include/domain/CircularOrbit.hpp`, `src/domain/CircularOrbit.cpp`), sem
MIXR, sem BT.CPP, testaveis isoladas (`tests/domain/test_CircularOrbit.cpp`):

- `groundTrack(OrbitElements, simTimeS)` -- o ponto sub-satelite num referencial FIXO NA TERRA
  (pronto para `convertGeod2Ecef()`/`setGeocPosition()`). A longitude ja subtrai
  `EARTH_ROTATION_RATE_RAD_S * simTimeS` -- o termo que converte a fase orbital (calculada num
  referencial inercial) para um referencial que gira junto com a Terra.
- `eciPosition(OrbitElements, simTimeS)` -- a mesma orbita, em Cartesiano INERCIAL (SEM a
  correcao de rotacao terrestre). Usada so' pela geometria de sombra (ver abaixo).

**Por que as duas funcoes precisam ficar SEPARADAS, nunca "a mesma formula menos um termo":** a
direcao do sol (`sunRightAscension`/`sunDeclination`) e' um vetor FIXO -- o MIXR nao tem
efemeride solar nenhuma, entao nao ha como derivar a posicao real do sol a partir de data/hora.
Um vetor fixo so' e' uma aproximacao HONESTA num referencial INERCIAL (o eixo Terra-Sol
verdadeiro muda de direcao devagar, ao longo do ano -- um vetor fixo no espaco inercial e' uma
simplificacao razoavel de curto prazo). Em ECEF (fixo-na-Terra), a direcao real do sol VARRE uma
volta completa a cada dia sideral -- um vetor "fixo" nesse referencial seria uma aproximacao
PIOR, nao inofensiva: alimentar a posicao fixa-na-Terra no teste de sombra faria o sol parecer
girar ao redor da Terra uma vez por dia sideral relativo ao satelite, com aparencia plausivel mas
numericamente errada.

**Simplificacao deliberada, e o motivo de nao afetar o teste de circularidade:** a conversao
final para ECEF (`base::nav::convertGeod2Ecef()`) trata a latitude/longitude calculadas por
`groundTrack()` como GEODETICAS sobre o elipsoide WGS84, quando na verdade a formula que as
produz assume uma Terra ESFERICA (mesma esfera que `MU_EARTH_M3_S2`/a orbita circular ja
assumem). Isso introduz um desvio geometrico pequeno (ordem de dezenas de km, uma fracao de
percentual do raio orbital de ~26.558 km) entre "onde a formula esferica diz que o satelite
esta" e "onde ele estaria de verdade sobre um elipsoide" -- irrelevante para o proposito
demonstrativo deste modelo, e nao vale a pena corrigir sem uma necessidade real (misturaria uma
Terra esferica na dinamica com uma elipsoidal na conversao, ganho de precisao questionavel). O
efeito NAO aparece no teste de altitude constante porque `convertGeod2Ecef()`/`convertEcef2Geod()`
sao EXATAMENTE inversas uma da outra (mesmo elipsoide, mesma formula) -- reler a altitude de
volta do `Player` sempre bate com o que foi configurado, a precisao de ponto flutuante, INDEPENDENTE
desse desvio geometrico de "onde" o ponto realmente esta.

### Escolha de projeto #1 -- fidelidade orbital: so' circular, sem excentricidade

GPS opera com orbitas quase circulares na pratica (excentricidade tipica 0,00-0,02) -- a
simplificacao nao custa realismo visivel para este modelo. Modelar excentricidade exigiria
resolver a equacao de Kepler (sem forma fechada -- um solver de Newton-Raphson) e trocar
`u = argLat0 + n*t` por uma relacao baseada em anomalia verdadeira. Adiado deliberadamente: sem
uma necessidade concreta de mostrar variacao perigeu/apogeu, o custo extra nao se paga. Fica
registrado aqui como extensao futura, nao como lacuna esquecida.

## Sol/sombra -- `domain::EclipseGeometry`

`sunState(satPosM, sunDirUnit, earthRadiusM)` -- modelo de sombra CILINDRICA (sem penumbra): a
Terra projeta um cilindro infinito de raio `earthRadiusM` ao longo do eixo anti-sol. Boa
aproximacao em escala MEO/LEO (o raio angular real do Sol, ~0,26 grau, faz o cone de umbra ser
quase cilindrico a essas distancias). Convencao de fronteira: `perpDist == earthRadiusM` conta
como `SUNLIT` (a borda fora da sombra), mesmo espirito inclusivo de `domain::ParachuteFsm`
(modelo paratrooper deste repositorio).

`sunRightAscension`/`sunDeclination` (slots de `Navstar3BtBehavior`) descrevem a direcao FIXA do
sol no referencial inercial -- sem variacao de estacao do ano, sem efemeride. E' o scenario que
escolhe a geometria (por exemplo, para garantir que uma janela de eclipse aconteca durante uma
janela de observacao curta).

### Escolha de projeto #2 -- o que a arvore de comportamento decide: eclipse/sol, nao um rotulo generico

Sem logica de combate nenhuma para inventar, a arvore decide o unico estado fisico genuino que
faz sentido para um satelite: `SUNLIT`/`ECLIPSE`. E' uma decisao real de duas alternativas (nao
um rotulo constante), derivada de geometria de verdade, e operacionalmente relevante (temporada
de eclipse afeta planejamento de energia/termico em satelites reais). A alternativa considerada
-- uma arvore minima de um no so, sempre `NOMINAL` (mesmo espirito do `flight_tree.xml` de
navegacao pura do C-130) -- foi descartada por nao dar a `tests/domain/`/`tests/tree/` nada de
verdade para afirmar: uma arvore que sempre diz a mesma coisa nao distingue "funcionando" de
"silenciosamente quebrada".

## A arvore

`Fallback` de 2 ramos, mesma forma do `flight_tree.xml` de producao (o primeiro `SUCCESS` vence):

```
1) na sombra (Sequence: IsEclipsed -> SetSunLabel "ECLIPSE")
2) fora da sombra (SetSunLabel "SUNLIT", incondicional -- ramo de degradacao)
```

`bt::DecisionContext` tem so' DOIS metodos (`sunState()`, `decision()`) -- ainda menor que a
versao de 3 metodos do C-130 (que ja reduziu a de 9 getters do A-4 por nao ter combate): este
modelo nao tem combustivel, alerta, contato ou navegacao por rota, so' o estado sol/sombra ja
calculado no ciclo. `IsEclipsedCondition` so' LE esse estado (nunca recalcula geometria) --
`Navstar3BtBehavior::genAction()` e' quem chama `domain::sunState()`, ANTES de tickar a arvore.

## O agente -- decide na fase 3, nunca no laco de fundo

`xnative::Navstar3AgentTC` e' uma copia estrutural de `models/players/paratrooper/src/xnative/
ParatrooperAgentTC.cpp` (que por sua vez adapta A-4/C-130), resolvendo as MESMAS tres armadilhas
de sempre: `AgentTC` nao e' construido por fabrica nenhuma do MIXR (registro manual);
`AgentTC::updateTC()` chama `controller()` em TODA fase sem filtro (aqui, filtrado para so' rodar
na fase 3, com o `dt` do frame inteiro); `Agent::updateData()` tambem chama `controller()` (aqui,
sobrescrito como no-op) -- sem essa terceira correcao, a orbita avancaria mais de uma vez por
frame.

**Tempo simulado acumulado, nunca relogio de parede.** `simTimeS_` (membro de
`Navstar3BtBehavior`) so' avanca por `dt` recebido em `genAction()` -- o mesmo `dt` do frame
inteiro que o agente ja recupera (`dt*4`). E' o que mantem a orbita reproduzivel entre 1, 2 e 4
threads de tempo critico: a mesma sequencia fixa de `dt` produz a MESMA sequencia de posicoes,
independente de quantas threads o pool tem. Medido: `tests/determinism/check_determinism.sh`
sobre `src/poc/navstar3-orbit`, 1200 frames, 1/2/4 threads T/C (mais a repeticao de 4), dumps
byte-identicos.

## Sem classe de "corpo fisico" propria -- `( SpaceVehicle )` e' nativa

Ao contrario do modelo paratrooper (que precisou de `xnative::Paratrooper`, derivada de `Effect`,
porque `mixr::models::factory.cpp` nao despacha `"Effect"` puro), `mixr::models::factory.cpp`
DESPACHA `"SpaceVehicle"` nativamente (`factory.cpp:180-181` do fork vendorizado) -- nenhuma
classe C++ nova precisou ser escrita para o corpo do satelite. So' quatro classes registradas:

| factory name | base class | papel |
|---|---|---|
| `Navstar3State` | `base::ubf::AbstractState` | percepcao |
| `Navstar3BtBehavior` | `base::ubf::AbstractBehavior` (+ `bt::DecisionContext`) | decisao -- dono da orbita, do tempo acumulado, da arvore |
| `Navstar3Action` | `base::ubf::AbstractAction` | atuacao -- aplica `setGeocPosition`/`setGeocVelocity` + xboard |
| `Navstar3AgentTC` | `base::ubf::AgentTC` | o agente, fase 3 do frame T/C |

`provides:` no `.edl` e' exatamente esses 4 nomes (`SpaceVehicle` vem da factory nativa do host,
nao deste `.so`). Prefixo `Navstar3` em todos, de proposito -- nomes de fabrica sao GLOBAIS ao
processo, nao por-plugin; `python3 tests/guard/check_colisao_fabrica.py` confirma zero colisao
contra A-4 (`Flight*`)/C-130 (`C130*`)/paratrooper (`Paratrooper*`)/template (`Example*`).

## `spd=` no dump/telemetria e' velocidade RELATIVA AO REFERENCIAL FIXO-NA-TERRA, nao inercial

Achado ao medir, nao antecipado: a velocidade escrita em `Navstar3BtBehavior::genAction()` vem de
diferenca finita entre posicoes ECEF (fixo-na-Terra) consecutivas -- ja que `groundTrack()`
subtrai a rotacao terrestre antes de converter para ECEF, essa velocidade e' a velocidade
RELATIVA AO REFERENCIAL EM ROTACAO, nao a velocidade orbital inercial (`v = sqrt(mu/a)`, ~3874
m/s para a orbita default). Medido rodando (`-deterministic`, proximo ao no ascendente):
`spd=3181 m/s`, nao ~3874 m/s -- a diferenca (~ velocidade de corrotacao da Terra projetada na
direcao do movimento, ~1937 m/s no equador, decomposta contra os componentes leste/norte da
velocidade inercial no cruzamento do equador com inclinacao 55 graus) bate com o valor calculado
a mao a partir da geometria orbital padrao (v_leste = v*cos(i), v_norte = v*sin(i) no no
ascendente). Nao e' um bug -- e' a definicao correta de velocidade ECEF, e vale registrar porque
um leitor comparando contra a formula ingenua de orbita circular esperaria ~3874 m/s.

## O que fica de fora, deliberadamente

- Excentricidade/anomalia verdadeira (ver "Escolha de projeto #1" acima).
- Perturbacao J2 (achatamento terrestre) e arrasto atmospheric -- irrelevantes a ~20.180 km de
  altitude (nao ha atmosfera residual significativa nessa cota) e fora do escopo demonstrativo.
- Pressao de radiacao solar.
- Efemeride solar real (data/hora -> posicao do sol) -- ver "Sol/sombra" acima.
- Atitude apontada para o nadir -- `setEulerAngles()` cosmetico para o Tacview, ideia registrada
  mas nao implementada (nenhum consumidor deste modelo precisa de atitude correta hoje).
- Constelacao completa -- o modelo aceita multiplas instancias (elementos orbitais sao slots por
  instancia), mas o cenario de producao (`src/poc/navstar3-orbit`) demonstra so' UM satelite,
  correspondendo ao pedido original ("um satelite... Navstar-3").

## Ler tambem

- [`../../players/paratrooper/docs/ARCHITECTURE.md`](../../players/paratrooper/docs/ARCHITECTURE.md)
  -- o agente de tempo critico e o padrao de tres armadilhas resolvidas, copiado aqui
- [`../../players/template/docs/CONTRATO.md`](../../players/template/docs/CONTRATO.md) -- o
  contrato completo que qualquer modelo, desta categoria ou de qualquer outra, tem que cumprir
- [`../../../CLAUDE.md`](../../../CLAUDE.md), secao "O MODELO e um plugin, construido numa etapa
  PREVIA" -- visao geral de `models/`
