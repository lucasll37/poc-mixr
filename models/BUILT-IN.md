# `models/BUILT-IN.md` — classes nativas do MIXR prontas para uso

Catálogo de referência, em português, de **toda classe nativa do MIXR que a cadeia de factories
deste repositório (`app/src/mixr_factory.cpp`) consegue de fato construir a partir de um `.edl`**
— ou seja, toda classe com um branch de despacho *real* (`new X()` alcançável) num dos
`factory.cpp` nativos: `mixr::models`, `mixr::simulation`, `mixr::terrain`, `mixr::dis`
(`interop::dis`), `mixr::linkage`, `mixr::recorder`, `mixr::base`. As libs próprias deste
repositório (`xplugin`/`xtacview`/`xclock`/`xjoystick`/`xmsg`) ficam de fora — não são MIXR
nativo.

**"Concreta" aqui não é "todo `IMPLEMENT_SUBCLASS` que existe no fonte".** O fork tem classes cujo
header foi incluído em `models/factory.cpp` mas nunca ganhou um branch de despacho — órfãs,
inalcançáveis via EDL apesar de compilarem — e classes genuinamente abstratas de infraestrutura.
Nenhuma das duas entra na tabela principal; as órfãs ganham uma nota própria (seção
`mixr::models`), e uma seção final cobre as classes abstratas que **são**, ainda assim, o ponto de
extensão oficial de um papel — quando só herança já resolve.

Também mostrada aqui: as duas classes com macro `IMPLEMENT_ABSTRACT_SUBCLASS` (`IrShape`,
`MultiActorAgent`) que, apesar do nome da macro, têm branch de despacho de verdade e são
`new`-construíveis — a macro só deixa `clone()` retornando `nullptr`, não impede a construção.
Entram na tabela principal como as demais.

**Método**: levantado por leitura direta do fonte vendorizado em `contexts/src/mixr/` (os
`factory.cpp` de cada módulo + os comentários `// Description:` dos headers) — não confiando no
catálogo já existente (`src/ui/edl_catalog.generated.json`), que superconta por não distinguir
branch de despacho real de `IMPLEMENT_*SUBCLASS` presente no fonte. Catálogo **manual, sem
regeneração automática por script** — se o fork do MIXR mudar de versão, precisa nova varredura
(mesmo espírito de "manual, sem enforcement automatizado" que `REGISTRO.md` já usa). Cada tabela
tem duas colunas de identificação: **Classe** (o nome C++, linkado para o `.hpp` correspondente em
`contexts/src/mixr/`) e **Fábrica** (a string usada no `.edl`, sempre mostrada mesmo quando igual à
classe — quando difere, é a primeira pista de que algo não é óbvio).

**Os links de `.hpp` só resolvem localmente.** `contexts/src/mixr/` inteiro é git-ignored (ver
CLAUDE.md, "Onde consultar o framework") — não vem num clone limpo, é cópia local da árvore de
fonte. Os links funcionam para quem já tem essa pasta populada (o caso normal de trabalho neste
repositório) e aparecem quebrados em qualquer visualização do repositório sem ela — GitHub
incluído. Se a pasta não existir, os headers instalados pelo Conan
(`~/.conan2/p/b/mixr*/p/include/mixr/...`) têm a mesma estrutura interna, só sem link direto daqui.

## Índice

- [`mixr::models`](#mixrmodels) — 96 classes
- [`mixr::simulation`](#mixrsimulation) — 2 classes
- [`mixr::terrain`](#mixrterrain) — 4 classes
- [`mixr::dis` (`interop::dis`)](#mixrdis-interopdis) — 3 classes
- [`mixr::linkage`](#mixrlinkage) — 11 classes
- [`mixr::recorder`](#mixrrecorder) — 9 classes
- [`mixr::base`](#mixrbase) — 100 classes
- [Classes abstratas de extensão](#classes-abstratas-de-extensão)
- [Eventos nativos do MIXR](#eventos-nativos-do-mixr) — `Component` (`event()`) e `REID_*` (Data Recorder)
- [Ler também](#ler-também)

**Total concreto: 225 classes.**

## `mixr::models`

96 classes, subdivididas por papel/categoria para caber numa leitura de varredura.

### DynamicsModel

| Classe | Fábrica | Observação |
|---|---|---|
| [`RacModel`](../contexts/src/mixr/include/mixr/models/dynamics/RacModel.hpp) | `RacModel` | Modelo de dinâmica de voo do "Robot Aircraft" (RAC) — dinâmica bem simples, derivado de `AerodynamicsModel`. |
| [`JSBSimModel`](../contexts/src/mixr/include/mixr/models/dynamics/JSBSimModel.hpp) | `JSBSimModel` | Modelo de dinâmica que embute o motor de voo JSBSim (`FGFDMExec`) como `AerodynamicsModel`. |
| [`LaeroModel`](../contexts/src/mixr/include/mixr/models/dynamics/LaeroModel.hpp) | `LaeroModel` | Modelo aerodinâmico simples e reconfigurável de 4 graus de liberdade, escrito por Larry Buckner. |

### Ambiente

| Classe | Fábrica | Observação |
|---|---|---|
| [`IrAtmosphere`](../contexts/src/mixr/include/mixr/models/environment/IrAtmosphere.hpp) | `IrAtmosphere` | Gerencia dados atmosféricos para calcular transmissividade e radiação de fundo em faixas de comprimento de onda infravermelho. |
| [`IrAtmosphere1`](../contexts/src/mixr/include/mixr/models/environment/IrAtmosphere1.hpp) | `IrAtmosphere1` | Gerencia dados atmosféricos (tabelas de radiação solar, de fundo e de transmissividade) para transmissividade e radiação de fundo em IR. |

### Mundo

| Classe | Fábrica | Observação |
|---|---|---|
| [`WorldModel`](../contexts/src/mixr/include/mixr/models/WorldModel.hpp) | `WorldModel` | Modelo do mundo espacial: earth model, referência de latitude/longitude e coordenadas da área de jogo (gaming area). |

### Bases de Player e veículos

| Classe | Fábrica | Observação |
|---|---|---|
| [`Player`](../contexts/src/mixr/include/mixr/models/player/Player.hpp) | `Player` | Interface abstrata para todo player da simulação (aeronave, veículo terrestre etc.) — apesar do nome, é concreta e alcançável via EDL. |
| [`AirVehicle`](../contexts/src/mixr/include/mixr/models/player/air/AirVehicle.hpp) | `AirVehicle` | Veículo aéreo genérico — base para aeronaves, com controles normalizados de manche, pedal, trim e freios. |
| [`Building`](../contexts/src/mixr/include/mixr/models/player/Building.hpp) | `Building` | Edificação genérica, especialização de `Player`. |
| [`GroundVehicle`](../contexts/src/mixr/include/mixr/models/player/ground/GroundVehicle.hpp) | `GroundVehicle` | Veículo terrestre genérico, com lançador (launcher) comandado por posição UP/DOWN/NONE. |
| [`LifeForm`](../contexts/src/mixr/include/mixr/models/player/LifeForm.hpp) | `LifeForm` | Forma de vida genérica (personagem/tropa), com estados de ação como em pé, andando, correndo, rastejando etc. |
| [`Ship`](../contexts/src/mixr/include/mixr/models/player/Ship.hpp) | `Ship` | Modelo genérico de navio, especialização de `Player`. |
| [`SpaceVehicle`](../contexts/src/mixr/include/mixr/models/player/space/SpaceVehicle.hpp) | `SpaceVehicle` | Veículo espacial genérico, com dados de combustível/motor e entradas de controle de translação e guinada. |
| [`Aircraft`](../contexts/src/mixr/include/mixr/models/player/air/Aircraft.hpp) | `Aircraft` | Aeronave genérica, especialização de `AirVehicle` sem comportamento adicional. |
| [`Helicopter`](../contexts/src/mixr/include/mixr/models/player/air/Helicopter.hpp) | `Helicopter` | Helicóptero genérico, especialização de `AirVehicle` sem comportamento adicional. |
| [`UnmannedAirVehicle`](../contexts/src/mixr/include/mixr/models/player/air/UnmannedAirVehicle.hpp) | `UnmannedAirVehicle` | Veículo aéreo não tripulado (UAV) genérico, especialização de `AirVehicle` sem comportamento adicional. |
| [`Tank`](../contexts/src/mixr/include/mixr/models/player/ground/Tank.hpp) | `Tank` | Tanque genérico, especialização de `GroundVehicle` sem comportamento adicional. |
| [`ArmoredVehicle`](../contexts/src/mixr/include/mixr/models/player/ground/ArmoredVehicle.hpp) | `ArmoredVehicle` | Veículo blindado genérico, especialização de `GroundVehicle` sem comportamento adicional. |
| [`WheeledVehicle`](../contexts/src/mixr/include/mixr/models/player/ground/WheeledVehicle.hpp) | `WheeledVehicle` | Veículo sobre rodas genérico, especialização de `GroundVehicle` sem comportamento adicional. |
| [`Artillery`](../contexts/src/mixr/include/mixr/models/player/ground/Artillery.hpp) | `Artillery` | Peça de artilharia genérica, especialização de `GroundVehicle` sem comportamento adicional. |
| [`SamVehicle`](../contexts/src/mixr/include/mixr/models/player/ground/SamVehicle.hpp) | `SamVehicle` | Veículo de defesa antiaérea (SAM, tipo TEL/TELAR), com alcance mínimo e máximo de lançamento de míssil. |
| [`GroundStation`](../contexts/src/mixr/include/mixr/models/player/ground/GroundStation.hpp) | `GroundStation` | Estação terrestre genérica, especialização de `GroundVehicle` sem comportamento adicional. |
| [`GroundStationRadar`](../contexts/src/mixr/include/mixr/models/player/ground/GroundStationRadar.hpp) | `GroundStationRadar` | Estação terrestre com radar, especialização de `GroundStation` sem comportamento adicional. |
| [`GroundStationUav`](../contexts/src/mixr/include/mixr/models/player/ground/GroundStationUav.hpp) | `GroundStationUav` | Estação terrestre de controle de UAV, especialização de `GroundStation` sem comportamento adicional. |
| [`MannedSpaceVehicle`](../contexts/src/mixr/include/mixr/models/player/space/MannedSpaceVehicle.hpp) | `MannedSpaceVehicle` | Veículo espacial tripulado genérico, especialização de `SpaceVehicle` sem comportamento adicional. |
| [`UnmannedSpaceVehicle`](../contexts/src/mixr/include/mixr/models/player/space/UnmannedSpaceVehicle.hpp) | `UnmannedSpaceVehicle` | Veículo espacial não tripulado genérico, especialização de `SpaceVehicle` com dinâmica própria. |
| [`BoosterSpaceVehicle`](../contexts/src/mixr/include/mixr/models/player/space/BoosterSpaceVehicle.hpp) | `BoosterSpaceVehicle` | Veículo espacial do tipo booster (estágio propulsor), especialização de `SpaceVehicle` sem comportamento adicional. |

### Sistemas, Pilot, Navigation, Datalink, Radio, Gimbal e OnboardComputer

Sete destes nomes (`Pilot`, `Navigation`, `Datalink`, `Radio`, `Gimbal`, `RfSensor` — ver seção de
sensores RF — e `OnboardComputer`) são também os ~10 papéis primários que
`Player::updateSystemPointers()` resolve por tipo (ver "Classes abstratas de extensão" no fim
deste documento) — já concretos aqui, usáveis direto ou como base para uma especialização própria.

| Classe | Fábrica | Observação |
|---|---|---|
| [`System`](../contexts/src/mixr/include/mixr/models/system/System.hpp) | `System` | Classe base para todo modelo de subsistema (system component) que pode ser anexado a um `Player`. |
| [`AvionicsPod`](../contexts/src/mixr/include/mixr/models/system/AvionicsPod.hpp) | `AvionicsPod` | Pod de aviônicos genérico, derivado de `ExternalStore`. |
| [`Pilot`](../contexts/src/mixr/include/mixr/models/system/Pilot.hpp) | `Pilot` | Modelo base de piloto — classe raiz para piloto, autopilot e lógica de decisão do piloto, para qualquer tipo de `Player`. |
| [`Autopilot`](../contexts/src/mixr/include/mixr/models/system/Autopilot.hpp) | `Autopilot` | Piloto automático com slots de hold de altitude, velocidade, rumo e modo de loiter/nav. |
| [`Navigation`](../contexts/src/mixr/include/mixr/models/navigation/Navigation.hpp) | `Navigation` | Sistema de navegação genérico: mantém posição atual, rotas, cues de guiagem e subsistemas NAV (INS, GPS) e NAVAIDS (TACAN, ILS). |
| [`Ins`](../contexts/src/mixr/include/mixr/models/navigation/Ins.hpp) | `Ins` | Dados genéricos de Sistema de Navegação Inercial (INS). |
| [`Gps`](../contexts/src/mixr/include/mixr/models/navigation/Gps.hpp) | `Gps` | Dados genéricos de Sistema de Posicionamento Global (GPS). |
| [`Route`](../contexts/src/mixr/include/mixr/models/navigation/Route.hpp) | `Route` | Gerenciador genérico de rota — mantém steerpoints, to/from, sequenciamento automático, dados de guiagem e ações de steerpoint. |
| [`Steerpoint`](../contexts/src/mixr/include/mixr/models/navigation/Steerpoint.hpp) | `Steerpoint` | Steerpoint genérico; contém dados posicionais e de navegação, com lista de steerpoints associados (FIX, OAP etc.). |
| [`TargetData`](../contexts/src/mixr/include/mixr/models/TargetData.hpp) | `TargetData` | Dados de perfil de alvo de uso geral, associáveis a um steerpoint de alvo ou a uma arma. |
| [`Bullseye`](../contexts/src/mixr/include/mixr/models/navigation/Bullseye.hpp) | `Bullseye` | Bullseye genérico, usado como ponto de referência; deriva de `Steerpoint`. |
| [`Datalink`](../contexts/src/mixr/include/mixr/models/system/Datalink.hpp) | `Datalink` | Classe base para todo modelo de datalink. |
| [`Gimbal`](../contexts/src/mixr/include/mixr/models/system/Gimbal.hpp) | `Gimbal` | Classe genérica para todo gimbal, antena RF, seeker IR etc. |
| [`ScanGimbal`](../contexts/src/mixr/include/mixr/models/system/ScanGimbal.hpp) | `ScanGimbal` | Modelo simples de gimbal: controle servo de taxa e posição, com varreduras de 1, 2 e 4 barras. |
| [`StabilizingGimbal`](../contexts/src/mixr/include/mixr/models/system/StabilizingGimbal.hpp) | `StabilizingGimbal` | Gimbal que tenta contrapor os movimentos de roll, pitch e yaw do player (estabilização por elevação, roll ou horizonte). |
| [`Antenna`](../contexts/src/mixr/include/mixr/models/system/Antenna.hpp) | `Antenna` | Modelo simples de antena: controle servo de taxa e posição, com varreduras de 1, 2 e 4 barras. |
| [`IrSeeker`](../contexts/src/mixr/include/mixr/models/system/IrSeeker.hpp) | `IrSeeker` | Modelo simples de seeker infravermelho. |
| [`OnboardComputer`](../contexts/src/mixr/include/mixr/models/system/OnboardComputer.hpp) | `OnboardComputer` | Classe base para todo sistema de computador de bordo; gerencia outros subsistemas e os track managers. |
| [`Radio`](../contexts/src/mixr/include/mixr/models/system/Radio.hpp) | `Radio` | Classe base para todo rádio (Comm, Nav, IFF etc.), com detecção por alcance/frequência ou por `RfSystem` completo. |
| [`CommRadio`](../contexts/src/mixr/include/mixr/models/system/CommRadio.hpp) | `CommRadio` | Classe genérica para todo modelo de rádio de comunicação. |
| [`Iff`](../contexts/src/mixr/include/mixr/models/system/Iff.hpp) | `Iff` | Classe genérica para sistemas IFF (o transponder SQUAWK), com os modos 1/2/3a/4a/4b/C. |

### Assinatura RF e sensores RF

| Classe | Fábrica | Observação |
|---|---|---|
| [`Gmti`](../contexts/src/mixr/include/mixr/models/sensor/Gmti.hpp) | `Gmti` | Modo de radar GMTI (Ground-Moving-Target-Indication) bem simples, derivado de `Radar`. |
| [`Stt`](../contexts/src/mixr/include/mixr/models/sensor/Stt.hpp) | `Stt` | Radar em modo Single-Target-Track (STT) simples, derivado de `Radar`. |
| [`Tws`](../contexts/src/mixr/include/mixr/models/sensor/Tws.hpp) | `Tws` | Radar em modo Track-While-Scan (TWS) simples, derivado de `Radar`. |
| [`SigConstant`](../contexts/src/mixr/include/mixr/models/Signatures.hpp) | `SigConstant` | Assinatura de RCS constante (`base::Number` ou `base::Decibel` em metros quadrados), via slot `rcs`. |
| [`SigSphere`](../contexts/src/mixr/include/mixr/models/Signatures.hpp) | `SigSphere` | Assinatura de RF de uma esfera simples, RCS calculado a partir do raio (slot `radius`). |
| [`SigPlate`](../contexts/src/mixr/include/mixr/models/Signatures.hpp) | `SigPlate` | Assinatura de RF de uma placa simples (comprimento `a` × largura `b`), sempre normal ao transmissor. |
| [`SigDihedralCR`](../contexts/src/mixr/include/mixr/models/Signatures.hpp) | `SigDihedralCR` | Assinatura de RF de um refletor de canto diedro, a partir do comprimento da aresta, sempre de frente para o transmissor. |
| [`SigTrihedralCR`](../contexts/src/mixr/include/mixr/models/Signatures.hpp) | `SigTrihedralCR` | Assinatura de RF de um refletor de canto triedro, a partir do comprimento da aresta, sempre de frente para o transmissor. |
| [`SigSwitch`](../contexts/src/mixr/include/mixr/models/Signatures.hpp) | `SigSwitch` | Alterna entre assinaturas de RF de subcomponentes conforme o `camouflageType` do ownship (1º par = tipo 0, 2º par = tipo 1 etc.). |
| [`SigAzEl`](../contexts/src/mixr/include/mixr/models/Signatures.hpp) | `SigAzEl` | Assinatura de RF por tabela de RCS (`base::Table2`) em função dos ângulos de azimute/elevação do alvo. |
| [`RfSensor`](../contexts/src/mixr/include/mixr/models/system/RfSensor.hpp) | `RfSensor` | Classe base para modelos de sensor R/F — interface comum e comportamento padrão para sensores de RF; um dos ~10 papéis primários de `Player`. |
| [`SensorMgr`](../contexts/src/mixr/include/mixr/models/system/SensorMgr.hpp) | `SensorMgr` | Gerencia uma lista de sensores R/F, derivada de `RfSensor`. |
| [`Radar`](../contexts/src/mixr/include/mixr/models/system/Radar.hpp) | `Radar` | Modelo de radar genérico; tipo de sensor R/F padrão identificado como `"RADAR"`. |
| [`Rwr`](../contexts/src/mixr/include/mixr/models/system/Rwr.hpp) | `Rwr` | Modelo de receptor de aviso de radar (Radar Warning Receiver), derivado de `RfSensor`. |
| [`Sar`](../contexts/src/mixr/include/mixr/models/system/Sar.hpp) | `Sar` | Modelo de SAR (Synthetic Aperture Radar) genérico, com slot de tamanho de chip (pixels) entre outros. |
| [`Jammer`](../contexts/src/mixr/include/mixr/models/system/Jammer.hpp) | `Jammer` | Jammer de exemplo; tipo de sensor R/F padrão identificado como `"JAMMER"`, derivado de `RfSensor`. |

### Assinatura IR e sensores IR

| Classe | Fábrica | Observação |
|---|---|---|
| [`IrSignature`](../contexts/src/mixr/include/mixr/models/IrSignature.hpp) | `IrSignature` | Classe base de assinaturas IR: forma da fonte, heat signature base, emissividade e área efetiva. |
| [`AircraftIrSignature`](../contexts/src/mixr/include/mixr/models/AircraftIrSignature.hpp) | `AircraftIrSignature` | Classe base para assinatura IR de aeronave, derivada de `IrSignature` — ⚠ derruba o processo se declarada sem as 6 tabelas (`getAirframeSignature()` desreferencia ponteiro nulo). |
| [`IrShape`](../contexts/src/mixr/include/mixr/models/IrShapes.hpp) | `IrShape` | Classe base para forma infravermelha — expõe a área efetiva (m²) usada no cálculo de assinatura IR; registrada com `IMPLEMENT_ABSTRACT_SUBCLASS`, mas ainda assim construível via EDL neste fork. |
| [`IrSphere`](../contexts/src/mixr/include/mixr/models/IrShapes.hpp) | `IrSphere` | Forma IR esférica (derivada de `IrShape`); área efetiva calculada a partir do slot `radius` (default 10). |
| [`IrBox`](../contexts/src/mixr/include/mixr/models/IrShapes.hpp) | `IrBox` | Forma IR em caixa retangular (derivada de `IrShape`), com dimensões x/y/z configuráveis (default 10 cada). |
| [`IrSensor`](../contexts/src/mixr/include/mixr/models/system/IrSensor.hpp) | `IrSensor` | Classe base para modelos de sensor IR — interface comum e comportamento padrão para sensores infravermelhos; um dos ~10 papéis primários de `Player`. |
| [`MergingIrSensor`](../contexts/src/mixr/include/mixr/models/system/MergingIrSensor.hpp) | `MergingIrSensor` | Sensor IR que só distingue alvos fora de um bin de az/el especificado; neste fork exige um `AirAngleOnlyTrkMgrPT` no `reset()` — classe órfã da factory (ver abaixo), beco sem saída aqui. |

### Track e gerentes de pista

| Classe | Fábrica | Observação |
|---|---|---|
| [`Track`](../contexts/src/mixr/include/mixr/models/Track.hpp) | `Track` | Representação genérica de uma pista/contato detectado, mantida por um `TrackManager`. |
| [`GmtiTrkMgr`](../contexts/src/mixr/include/mixr/models/system/trackmanager/GmtiTrkMgr.hpp) | `GmtiTrkMgr` | Gerenciador de pistas (Track Manager) bem simples para GMTI (Ground Moving Target Indication). |
| [`AirTrkMgr`](../contexts/src/mixr/include/mixr/models/system/trackmanager/AirTrkMgr.hpp) | `AirTrkMgr` | Gerenciador de pistas para modos ar-ar (ex.: TWS, ACM, SST). |
| [`RwrTrkMgr`](../contexts/src/mixr/include/mixr/models/system/trackmanager/RwrTrkMgr.hpp) | `RwrTrkMgr` | Gerenciador de pistas para o RWR (Radar Warning Receiver). |
| [`AirAngleOnlyTrkMgr`](../contexts/src/mixr/include/mixr/models/system/trackmanager/AirAngleOnlyTrkMgr.hpp) | `AirAngleOnlyTrkMgr` | Gerenciador de pistas para modos ar-ar, baseado só em ângulo (angle-only). |

### Agente UBF e diversos

| Classe | Fábrica | Observação |
|---|---|---|
| [`SimAgent`](../contexts/src/mixr/include/mixr/models/SimAgent.hpp) | `SimAgent` | Agente que gerencia um componente (o "actor", player ou componente de player) com um behavior UBF. |
| [`MultiActorAgent`](../contexts/src/mixr/include/mixr/models/MultiActorAgent.hpp) | `MultiActorAgent` | Agente genérico para controlar uma lista de actors, cada um com seu próprio behavior; registrada com `IMPLEMENT_ABSTRACT_SUBCLASS`, mas ainda assim construível via EDL — só faz sentido quando há estado compartilhado entre os actors. |
| [`CollisionDetect`](../contexts/src/mixr/include/mixr/models/system/CollisionDetect.hpp) | `CollisionDetect` | Componente de detecção de colisão: adicionado a um player (ownship), procura colisões com outros players da simulação e dispara `CRASH_EVENT` nos dois envolvidos. |

### Ações de steerpoint

| Classe | Fábrica | Observação |
|---|---|---|
| [`ActionImagingSar`](../contexts/src/mixr/include/mixr/models/Actions.hpp) | `ActionImagingSar` | Ação de steerpoint: dispara captura SAR, roda o ATR e baixa a imagem resultante; slots de posição/elevação do alvo, resolução e tamanho da imagem. |
| [`ActionWeaponRelease`](../contexts/src/mixr/include/mixr/models/Actions.hpp) | `ActionWeaponRelease` | Ação de steerpoint que lança uma arma; apesar do slot `station`, `trigger()` ignora-o e sempre libera a primeira `Bomb` livre da lista. |
| [`ActionDecoyRelease`](../contexts/src/mixr/include/mixr/models/Actions.hpp) | `ActionDecoyRelease` | Ação de steerpoint que libera um ou mais decoys, com slots de quantidade a lançar e intervalo entre lançamentos. |
| [`ActionCamouflageType`](../contexts/src/mixr/include/mixr/models/Actions.hpp) | `ActionCamouflageType` | Ação de steerpoint que, ao disparar, troca o tipo de camuflagem do próprio ownship (slot `camouflageType`). |

### Stores, armas e efeitos

| Classe | Fábrica | Observação |
|---|---|---|
| [`Stores`](../contexts/src/mixr/include/mixr/models/system/Stores.hpp) | `Stores` | Container genérico de artefatos externos (lançador, rack, ejetor, pilone etc.), agregando armas e outros stores por número de estação. |
| [`SimpleStoresMgr`](../contexts/src/mixr/include/mixr/models/system/SimpleStoresMgr.hpp) | `StoresMgr` | Gerenciador de estações de armas de exemplo — só esta subclasse concreta é construível via EDL, não a base abstrata `StoresMgr` (ver classes órfãs abaixo). |
| [`FuelTank`](../contexts/src/mixr/include/mixr/models/system/FuelTank.hpp) | `FuelTank` | Tanque de combustível genérico (`ExternalStore`), com slots de peso de combustível e capacidade do tanque (lb). |
| [`Gun`](../contexts/src/mixr/include/mixr/models/system/Guns.hpp) | `Gun` | Classe base de canhões (`ExternalStore` fixo, nunca vira player independente), com slots de tipo de bala, cadência de tiro e posição no ownship. |
| [`Bomb`](../contexts/src/mixr/include/mixr/models/player/weapon/Bomb.hpp) | `Bomb` | Classe base de bombas: modelo simples de balística e guiagem, com slots de opção de armamento (arming), espoletas (fuzes) e índice de arrasto. |
| [`Missile`](../contexts/src/mixr/include/mixr/models/player/weapon/Missile.hpp) | `Missile` | Classe base de mísseis, com modelo default simples de dinâmica (velocidade/G máximos) e comandos de rumo/arfagem/velocidade. |
| [`Aam`](../contexts/src/mixr/include/mixr/models/player/weapon/Aam.hpp) | `AamMissile` | Míssil Ar-Ar derivado de `Missile`, com modelo simples de aerodinâmica/guiagem que subclasses podem sobrescrever. |
| [`Agm`](../contexts/src/mixr/include/mixr/models/player/weapon/Agm.hpp) | `AgmMissile` | Míssil Ar-Solo derivado de `Missile`, com modelo simples de aerodinâmica/guiagem que subclasses podem sobrescrever. |
| [`Sam`](../contexts/src/mixr/include/mixr/models/player/weapon/Sam.hpp) | `Sam` | Classe base para mísseis Superfície-Ar (SAM), derivada de `Missile`. |
| [`Bullet`](../contexts/src/mixr/include/mixr/models/player/weapon/Bullet.hpp) | `Bullet` | Descreve a munição usada para criar o player de "flyout" de uma arma; durante o voo os projéteis são agrupados em rajadas (bursts). |
| [`Chaff`](../contexts/src/mixr/include/mixr/models/player/effect/Chaff.hpp) | `Chaff` | Classe genérica de chaff (contramedida de radar), derivada de `Effect`. |
| [`Decoy`](../contexts/src/mixr/include/mixr/models/player/effect/Decoy.hpp) | `Decoy` | Classe genérica de decoy (chamariz/engodo), derivada de `Effect`. |
| [`Flare`](../contexts/src/mixr/include/mixr/models/player/effect/Flare.hpp) | `Flare` | Classe genérica de flare (contramedida infravermelha), derivada de `Effect`. |

### Classes órfãs (não alcançáveis via EDL)

Header incluído em `contexts/src/mixr/src/models/factory.cpp`, `IMPLEMENT_SUBCLASS` presente no
`.cpp` — compilam normalmente — mas **sem branch de despacho**: `mixrFactory()` nunca as
constrói a partir de um `.edl`. Ficam de fora da tabela principal; registradas aqui só para quem
for procurar e não encontrar.

| Classe | Fábrica | Por que fica de fora |
|---|---|---|
| [`AirAngleOnlyTrkMgrPT`](../contexts/src/mixr/include/mixr/models/system/trackmanager/AirAngleOnlyTrkMgrPT.hpp) | `AirAngleOnlyTrkMgrPT` | Sem branch de despacho — é a dependência que torna `MergingIrSensor` um beco sem saída (ver acima). |
| [`ExternalStore`](../contexts/src/mixr/include/mixr/models/system/ExternalStore.hpp) | `ExternalStore` | Sem branch de despacho. |
| [`StoresMgr`](../contexts/src/mixr/include/mixr/models/system/StoresMgr.hpp) | `BaseStoresMgr` | A base abstrata real do gerente de stores; só a subclasse concreta `SimpleStoresMgr` (fábrica `StoresMgr`) é alcançável. |
| [`Action`](../contexts/src/mixr/include/mixr/models/Actions.hpp) | `Action` | Sem branch de despacho — só as 4 subclasses concretas (`ActionImagingSar` etc.) são alcançáveis. |
| [`RfTrack`](../contexts/src/mixr/include/mixr/models/Track.hpp) | `RfTrack` | Declarada junto de `Track` (mesmo `.cpp`), mas sem branch de despacho. |
| [`IrTrack`](../contexts/src/mixr/include/mixr/models/Track.hpp) | `IrTrack` | Idem — declarada junto de `Track`, sem branch de despacho. |

## `mixr::simulation`

| Classe | Fábrica | Observação |
|---|---|---|
| [`Simulation`](../contexts/src/mixr/include/mixr/simulation/Simulation.hpp) | `Simulation` | Executivo da simulação: gerencia a lista de players do mundo simulado e a execução do frame de tempo crítico/fundo. |
| [`Station`](../contexts/src/mixr/include/mixr/simulation/Station.hpp) | `Station` | Container de topo da aplicação: agrega `Simulation`, `dataRecorder`, `networks`, `ioHandler` e controla as threads de tempo crítico/fundo/rede. |

## `mixr::terrain`

Não é encadeada por nenhuma outra factory nativa — sem `mixr::terrain::factory(name)` no
`mixr_factory.cpp` do host, `( SrtmHgtFile ... )` do `.edl` não constrói nada, em silêncio (ver
CLAUDE.md, seção "Terreno").

| Classe | Fábrica | Observação |
|---|---|---|
| [`QuadMap`](../contexts/src/mixr/include/mixr/terrain/QuadMap.hpp) | `QuadMap` | Gerencia até 4 arquivos de elevação de terreno organizados num padrão 2×2. |
| [`DedFile`](../contexts/src/mixr/include/mixr/terrain/ded/DedFile.hpp) | `DedFile` | Carrega arquivo de elevação Digital Elevation Data (DED) da MultiGen, Inc. |
| [`DtedFile`](../contexts/src/mixr/include/mixr/terrain/dted/DtedFile.hpp) | `DtedFile` | Carrega dado de elevação no formato DTED. |
| [`SrtmHgtFile`](../contexts/src/mixr/include/mixr/terrain/srtm/SrtmHgtFile.hpp) | `SrtmHgtFile` | Carrega dado de elevação SRTM (`.hgt`), adaptado de `terrain::DtedFile` — não lê `.gz` e valida o tamanho exato em bytes. |

## `mixr::dis` (`interop::dis`)

Namespace real é `mixr::dis`, apesar do caminho do header ser `mixr/interop/dis/`. Só esta árvore
é encadeada pelo host — `interop::common`, `interop::hla` e `interop::rprfom` não entram (nomes
colidentes com classes reais, ex. `Aircraft`/`NetIO`/`Ntm` próprios).

| Classe | Fábrica | Observação |
|---|---|---|
| [`NetIO`](../contexts/src/mixr/include/mixr/interop/dis/NetIO.hpp) | `DisNetIO` | Gerenciador do protocolo DIS (Distributed Interactive Simulation) — entrada/saída de PDUs pela rede. |
| [`Ntm`](../contexts/src/mixr/include/mixr/interop/dis/Ntm.hpp) | `DisNtm` | Mapeador de tipo de rede DIS: converte entre tipos de player MIXR e códigos de entidade DIS. |
| [`EmissionPduHandler`](../contexts/src/mixr/include/mixr/interop/dis/EmissionPduHandler.hpp) | `EmissionPduHandler` | Trata a entrada/saída DIS de um sistema emissor (ex.: `RfSensor`) via PDUs de emissão eletromagnética. |

## `mixr::linkage`

Base do `xjoystick` deste repositório (`mixr::linkage::IoHandler`/`IoData`/`IoDevice`/adapters).
Não é encadeada por nenhuma outra factory nativa — mesma armadilha do `terrain` acima.

| Classe | Fábrica | Observação |
|---|---|---|
| [`IoData`](../contexts/src/mixr/include/mixr/linkage/IoData.hpp) | `IoData` | Buffer genérico de dados de E/S; o usuário define o número de canais de cada tipo (AI/AO/DI/DO). |
| [`DiscreteInput`](../contexts/src/mixr/include/mixr/linkage/adapters/DiscreteInput.hpp) | `DiscreteInput` | Adaptador que gerencia uma entrada discreta (DI), mapeando um canal do dispositivo para um canal do `IoData`. |
| [`DiscreteOutput`](../contexts/src/mixr/include/mixr/linkage/adapters/DiscreteOutput.hpp) | `DiscreteOutput` | Adaptador que gerencia uma saída discreta (DO), mapeando um canal do `IoData` para um canal do dispositivo. |
| [`AnalogInput`](../contexts/src/mixr/include/mixr/linkage/adapters/AnalogInput.hpp) | `AnalogInput` | Adaptador que gerencia uma entrada analógica (AI), com escala/offset entre o valor bruto do dispositivo e o canal do `IoData`. |
| [`AnalogOutput`](../contexts/src/mixr/include/mixr/linkage/adapters/AnalogOutput.hpp) | `AnalogOutput` | Adaptador que gerencia uma saída analógica (AO), com escala/offset entre o canal do `IoData` e o valor do dispositivo. |
| [`Ai2DiSwitch`](../contexts/src/mixr/include/mixr/linkage/adapters/Ai2DiSwitch.hpp) | `Ai2DiSwitch` | Conversor de sinal: liga uma DI para `true` quando uma entrada analógica atinge um nível (com opção de inverter). |
| [`AnalogInputFixed`](../contexts/src/mixr/include/mixr/linkage/generators/AnalogInputFixed.hpp) | `AnalogInputFixed` | Gerador de sinal de entrada analógica que produz sempre um valor fixo. |
| [`AnalogSignalGen`](../contexts/src/mixr/include/mixr/linkage/generators/AnalogSignalGen.hpp) | `AnalogSignalGen` | Gerador de sinal de entrada analógica: ondas seno, cosseno, quadrada e dente de serra. |
| [`DiscreteInputFixed`](../contexts/src/mixr/include/mixr/linkage/generators/DiscreteInputFixed.hpp) | `DiscreteInputFixed` | Gerador de sinal de entrada discreta que produz sempre "ligado" ou "desligado" fixo. |
| [`MockDevice`](../contexts/src/mixr/include/mixr/linkage/MockDevice.hpp) | `MockDevice` | Dispositivo de E/S falso (não físico): descarta saídas como um NULL e gera entradas por uma lista de geradores de sinal. |
| [`UsbJoystick`](../contexts/src/mixr/src/linkage/platform/UsbJoystick_linux.hpp) | `UsbJoystick` | `IoDevice` concreto para joystick USB físico; no Linux lê `/dev/js%d` ou `/dev/input/js%d` via ioctl/read não bloqueante. |

## `mixr::recorder`

O schema `DataRecord.proto` por trás é fechado (sem campo de texto livre) — ver `libs/xlog`/
`libs/xmsg` no CLAUDE.md para o porquê deste repo não usa `mixr::recorder` para tudo.

| Classe | Fábrica | Observação |
|---|---|---|
| [`FileWriter`](../contexts/src/mixr/include/mixr/recorder/FileWriter.hpp) | `FileWriter` | Serializa e grava em arquivo os dados de um `DataRecord` de protocol buffer. |
| [`FileReader`](../contexts/src/mixr/include/mixr/recorder/FileReader.hpp) | `RecorderFileReader` | Lê e faz parse de registros de dados a partir de um arquivo — nome de fábrica próprio (`RecorderFileReader`, não `FileReader`) justamente para não colidir com `base::FileReader`. |
| [`NetInput`](../contexts/src/mixr/include/mixr/recorder/NetInput.hpp) | `NetInput` | Lê e faz parse de registros de dados a partir de um fluxo de rede. |
| [`NetOutput`](../contexts/src/mixr/include/mixr/recorder/NetOutput.hpp) | `NetOutput` | Serializa e envia um `DataRecord` pela rede. |
| [`OutputHandler`](../contexts/src/mixr/include/mixr/recorder/OutputHandler.hpp) | `OutputHandler` | Handler genérico de saída para registros de dados de protocol buffer; subclasses implementam `processRecordImp()`. |
| [`TabPrinter`](../contexts/src/mixr/include/mixr/recorder/TabPrinter.hpp) | `TabPrinter` | Imprime os dados de um `DataRecord` de protocol buffer em formato tabular. |
| [`PrintPlayer`](../contexts/src/mixr/include/mixr/recorder/PrintPlayer.hpp) | `PrintPlayer` | Imprime os dados de estado de um player a partir do registro gravado. |
| [`DataRecorder`](../contexts/src/mixr/include/mixr/recorder/DataRecorder.hpp) | `DataRecorder` | Gravador de dados: recebe e processa amostras de dados da simulação, encaminhando aos `OutputHandler` configurados. |
| [`PrintSelected`](../contexts/src/mixr/include/mixr/recorder/PrintSelected.hpp) | `PrintSelected` | Imprime apenas os dados selecionados de uma mensagem de registro. |

## `mixr::base`

100 classes — a camada de primitivas/unidades/infra que o parser EDL precisa para literais como
`( Meters 5.0 )`, mais alguns componentes de infraestrutura. Subdividida por família.

### Números

| Classe | Fábrica | Observação |
|---|---|---|
| [`Number`](../contexts/src/mixr/include/mixr/base/numeric/Number.hpp) | `Number` | Classe base para objetos numéricos (`Float`, `Integer`, `Boolean`, `Decibel`, `LatLon`) e para os operadores `Add`/`Subtract`/`Multiply`/`Divide`. |
| [`Complex`](../contexts/src/mixr/include/mixr/base/numeric/Complex.hpp) | `Complex` | Número complexo genérico: parte real herdada de `Number` mais parte imaginária (slot `imag`), com magnitude e argumento fasorial. |
| [`Integer`](../contexts/src/mixr/include/mixr/base/numeric/Integer.hpp) | `int` | Classe para números inteiros, com operadores de atribuição (`+=`, `-=`, `*=`, `/=`, `%=`) equivalentes ao C++. |
| [`Float`](../contexts/src/mixr/include/mixr/base/numeric/Float.hpp) | `float` | Classe para números de ponto flutuante (double), com operadores de conversão `float()`/`double()`. |
| [`Boolean`](../contexts/src/mixr/include/mixr/base/numeric/Boolean.hpp) | `boolean` | Classe para valores booleanos, com operador de conversão `bool()`. |
| [`Decibel`](../contexts/src/mixr/include/mixr/base/units/Decibel.hpp) | `dB` | Container de decibéis (`db = 10*log10(valor)`); mantém e opera o número sempre na escala em dB. |
| [`LatLon`](../contexts/src/mixr/include/mixr/base/LatLon.hpp) | `LatLon` | Número de latitude ou longitude, composto por direção (N/S/E/W), graus, minutos e segundos. |
| [`Add`](../contexts/src/mixr/include/mixr/base/numeric/Operators.hpp) | `+` | Operador numérico de soma: `(+ val n2)` devolve `val` mais `n2` (aceita até 10 operandos via slots `n2..n10`). |
| [`Subtract`](../contexts/src/mixr/include/mixr/base/numeric/Operators.hpp) | `-` | Operador numérico de subtração: `(- val n2)` devolve `val` menos `n2`. |
| [`Multiply`](../contexts/src/mixr/include/mixr/base/numeric/Operators.hpp) | `*` | Operador numérico de multiplicação: `(* val n2)` devolve `val` multiplicado por `n2`. |
| [`Divide`](../contexts/src/mixr/include/mixr/base/numeric/Operators.hpp) | `/` | Operador numérico de divisão: `(/ val n2)` devolve `val` dividido por `n2`; divisor zero faz `operation()` não fazer nada. |

### Componentes

| Classe | Fábrica | Observação |
|---|---|---|
| [`FileReader`](../contexts/src/mixr/include/mixr/base/FileReader.hpp) | `FileReader` | Gerencia a leitura de arquivos com registros de tamanho fixo, configurados por `pathname`/`filename`/`recordLength` — classe distinta de `mixr::recorder::FileReader` (mesmo nome, namespace diferente). |
| [`Statistic`](../contexts/src/mixr/include/mixr/base/Statistic.hpp) | `Statistic` | Calculadora estatística: média, média absoluta, variância, desvio padrão, RMS, máximo e mínimo dos pontos adicionados via `sigma()`. |

### Transformações

| Classe | Fábrica | Observação |
|---|---|---|
| [`Translation`](../contexts/src/mixr/include/mixr/base/Transforms.hpp) | `Translation` | Transformação geométrica de translação: desloca X/Y (opcionalmente Z) pelos fatores dados nos slots x/y/z. |
| [`Rotation`](../contexts/src/mixr/include/mixr/base/Transforms.hpp) | `Rotation` | Transformação geométrica de rotação: gira `w` radianos em torno do eixo Z, ou em torno do vetor (x,y,z) com 4 parâmetros. |
| [`Scale`](../contexts/src/mixr/include/mixr/base/Transforms.hpp) | `Scale` | Transformação geométrica de escala: multiplica X/Y (opcionalmente Z) pelos fatores dados nos slots x/y/z. |

### Functors e tabelas

| Classe | Fábrica | Observação |
|---|---|---|
| [`Func1`](../contexts/src/mixr/include/mixr/base/functors/Func1.hpp) | `Func1` | Função/functor genérico unidimensional: `f(iv1)`, tipicamente apoiado numa `Table1`. |
| [`Func2`](../contexts/src/mixr/include/mixr/base/functors/Func2.hpp) | `Func2` | Função/functor genérico bidimensional: `f(iv1, iv2)`. |
| [`Func3`](../contexts/src/mixr/include/mixr/base/functors/Func3.hpp) | `Func3` | Função/functor genérico tridimensional: `f(iv1, iv2, iv3)`. |
| [`Func4`](../contexts/src/mixr/include/mixr/base/functors/Func4.hpp) | `Func4` | Função/functor genérico de 4 dimensões: `f(iv1, iv2, iv3, iv4)`. |
| [`Func5`](../contexts/src/mixr/include/mixr/base/functors/Func5.hpp) | `Func5` | Função/functor genérico de 5 dimensões: `f(iv1, iv2, iv3, iv4, iv5)`. |
| [`Polynomial`](../contexts/src/mixr/include/mixr/base/functors/Polynomial.hpp) | `Polynomial` | Função polinomial `f(x) = a0 + a1·x + a2·x² + ... + aN·xN`, com coeficientes definidos pelo slot `coefficients`. |
| [`Table1`](../contexts/src/mixr/include/mixr/base/functors/Table1.hpp) | `Table1` | Tabela de dados para interpolação linear (LFI) unidimensional, com pontos da variável independente no slot `x`. |
| [`Table2`](../contexts/src/mixr/include/mixr/base/functors/Table2.hpp) | `Table2` | Tabela de dados para interpolação linear (LFI) bidimensional — `data:` é lista de listas, uma sublista por ponto de `y`. |
| [`Table3`](../contexts/src/mixr/include/mixr/base/functors/Table3.hpp) | `Table3` | Tabela de dados para interpolação linear (LFI) tridimensional. |
| [`Table4`](../contexts/src/mixr/include/mixr/base/functors/Table4.hpp) | `Table4` | Tabela de dados para interpolação linear (LFI) de 4 dimensões. |
| [`Table5`](../contexts/src/mixr/include/mixr/base/functors/Table5.hpp) | `Table5` | Tabela de dados para interpolação linear (LFI) de 5 dimensões. |

### Timers

| Classe | Fábrica | Observação |
|---|---|---|
| [`UpTimer`](../contexts/src/mixr/include/mixr/base/Timers.hpp) | `UpTimer` | Temporizador de propósito geral cujo tempo conta na direção positiva (crescente). |
| [`DownTimer`](../contexts/src/mixr/include/mixr/base/Timers.hpp) | `DownTimer` | Temporizador de propósito geral cujo tempo conta na direção negativa (decrescente). |

### UBF

| Classe | Fábrica | Observação |
|---|---|---|
| [`ubf::Agent`](../contexts/src/mixr/include/mixr/base/ubf/Agent.hpp) | `UbfAgent` | Agente genérico que controla um ator (player ou componente) via `updateData()`, delegando a decisão aos slots `state`/`behavior`. |
| [`ubf::Arbiter`](../contexts/src/mixr/include/mixr/base/ubf/Arbiter.hpp) | `UbfArbiter` | Meta-behavior UBF que arbitra uma lista de behaviors (slot `behaviors`) e gera a ação com maior voto por padrão. |

### Ângulos

| Classe | Fábrica | Observação |
|---|---|---|
| [`Degrees`](../contexts/src/mixr/include/mixr/base/units/Angles.hpp) | `Degrees` | Unidade de ângulo: graus, definida como `Angle * 180.0` (equivale a `Semicircles` convertido). |
| [`Radians`](../contexts/src/mixr/include/mixr/base/units/Angles.hpp) | `Radians` | Unidade de ângulo: radianos, definida como `Angle * PI`. |
| [`Semicircles`](../contexts/src/mixr/include/mixr/base/units/Angles.hpp) | `Semicircles` | Unidade base de ângulo: semicírculos; uma instância com valor 1.0 é a unidade base para ângulos. |

### Áreas

| Classe | Fábrica | Observação |
|---|---|---|
| [`SquareMeters`](../contexts/src/mixr/include/mixr/base/units/Areas.hpp) | `SquareMeters` | Unidade base de área: metros quadrados; uma instância com valor 1.0 é a unidade base. |
| [`SquareFeet`](../contexts/src/mixr/include/mixr/base/units/Areas.hpp) | `SquareFeet` | Unidade de área: pés quadrados, equivalente a metros quadrados × 10,76391. |
| [`SquareInches`](../contexts/src/mixr/include/mixr/base/units/Areas.hpp) | `SquareInches` | Unidade de área: polegadas quadradas, equivalente a metros quadrados × 1550,0030399. |
| [`SquareYards`](../contexts/src/mixr/include/mixr/base/units/Areas.hpp) | `SquareYards` | Unidade de área: jardas quadradas, equivalente a metros quadrados × 1,19599. |
| [`SquareMiles`](../contexts/src/mixr/include/mixr/base/units/Areas.hpp) | `SquareMiles` | Unidade de área: milhas quadradas, equivalente a metros quadrados × 0,00000038610216. |
| [`SquareCentiMeters`](../contexts/src/mixr/include/mixr/base/units/Areas.hpp) | `SquareCentiMeters` | Unidade de área: centímetros quadrados, equivalente a metros quadrados × 10.000,0. |
| [`SquareMilliMeters`](../contexts/src/mixr/include/mixr/base/units/Areas.hpp) | `SquareMilliMeters` | Unidade de área: milímetros quadrados, equivalente a metros quadrados × 1.000.000,0. |
| [`SquareKiloMeters`](../contexts/src/mixr/include/mixr/base/units/Areas.hpp) | `SquareKiloMeters` | Unidade de área: quilômetros quadrados, equivalente a metros quadrados × 0,000001. |
| [`DecibelSquareMeters`](../contexts/src/mixr/include/mixr/base/units/Areas.hpp) | `DecibelSquareMeters` | Unidade de área em decibéis: `10·log10(metros quadrados)`; usada tipicamente para seção reta radar (RCS). |

### Distâncias

| Classe | Fábrica | Observação |
|---|---|---|
| [`Meters`](../contexts/src/mixr/include/mixr/base/units/Distances.hpp) | `Meters` | Unidade base de distância: metros; uma instância com valor 1.0 é a unidade base. |
| [`CentiMeters`](../contexts/src/mixr/include/mixr/base/units/Distances.hpp) | `CentiMeters` | Unidade de distância: centímetros, equivalente a Metros × 100,0. |
| [`MicroMeters`](../contexts/src/mixr/include/mixr/base/units/Distances.hpp) | `MicroMeters` | Unidade de distância: micrômetros, equivalente a Metros × 1.000.000,0. |
| [`Microns`](../contexts/src/mixr/include/mixr/base/units/Distances.hpp) | `Microns` | Unidade de distância: microns (sinônimo de `MicroMeters`), equivalente a Metros × 1.000.000,0. |
| [`KiloMeters`](../contexts/src/mixr/include/mixr/base/units/Distances.hpp) | `KiloMeters` | Unidade de distância: quilômetros, equivalente a Metros / 1000,0. |
| [`Inches`](../contexts/src/mixr/include/mixr/base/units/Distances.hpp) | `Inches` | Unidade de distância: polegadas, equivalente a Metros / 0,0254. |
| [`Feet`](../contexts/src/mixr/include/mixr/base/units/Distances.hpp) | `Feet` | Unidade de distância: pés, equivalente a Metros / 0,3048. |
| [`NauticalMiles`](../contexts/src/mixr/include/mixr/base/units/Distances.hpp) | `NauticalMiles` | Unidade de distância: milhas náuticas, equivalente a Metros × 1851,999942. |
| [`StatuteMiles`](../contexts/src/mixr/include/mixr/base/units/Distances.hpp) | `StatuteMiles` | Unidade de distância: milhas terrestres, equivalente a Metros × 1609,34313095. |

### Energias

| Classe | Fábrica | Observação |
|---|---|---|
| [`KiloWattHours`](../contexts/src/mixr/include/mixr/base/units/Energies.hpp) | `KiloWattHours` | Unidade de energia: quilowatt-hora, equivalente a Joules × 0,000000277778. |
| [`BTUs`](../contexts/src/mixr/include/mixr/base/units/Energies.hpp) | `BTUs` | Unidade de energia: unidade térmica britânica (BTU), equivalente a Joules × 9,478e-04. |
| [`Calories`](../contexts/src/mixr/include/mixr/base/units/Energies.hpp) | `Calories` | Unidade de energia: calorias, equivalente a Joules × 0,2388888888888888889. |
| [`FootPounds`](../contexts/src/mixr/include/mixr/base/units/Energies.hpp) | `FootPounds` | Unidade de energia: pé-libra (foot-pound), equivalente a Joules × 0,7376. |
| [`Joules`](../contexts/src/mixr/include/mixr/base/units/Energies.hpp) | `Joules` | Unidade base de energia: joules; uma instância com valor 1.0 é a unidade base. |

### Forças

| Classe | Fábrica | Observação |
|---|---|---|
| [`Newtons`](../contexts/src/mixr/include/mixr/base/units/Forces.hpp) | `Newtons` | Unidade base de força: newtons; uma instância com valor 1.0 é a unidade base. |
| [`KiloNewtons`](../contexts/src/mixr/include/mixr/base/units/Forces.hpp) | `KiloNewtons` | Unidade de força: quilonewtons, equivalente a Newtons × 1000,0. |
| [`Poundals`](../contexts/src/mixr/include/mixr/base/units/Forces.hpp) | `Poundals` | Unidade de força: poundal, equivalente a Newtons × 7,23301. |
| [`PoundForces`](../contexts/src/mixr/include/mixr/base/units/Forces.hpp) | `PoundForces` | Unidade de força: libra-força (pound-force), equivalente a Newtons × 0,224809. |

### Frequências

| Classe | Fábrica | Observação |
|---|---|---|
| [`Hertz`](../contexts/src/mixr/include/mixr/base/units/Frequencies.hpp) | `Hertz` | Unidade base de frequência: hertz; uma instância com valor 1.0 é a unidade base. |
| [`KiloHertz`](../contexts/src/mixr/include/mixr/base/units/Frequencies.hpp) | `KiloHertz` | Unidade de frequência: quilohertz, equivalente a Hertz × 0,001. |
| [`MegaHertz`](../contexts/src/mixr/include/mixr/base/units/Frequencies.hpp) | `MegaHertz` | Unidade de frequência: megahertz, equivalente a Hertz × 0,000001. |
| [`GigaHertz`](../contexts/src/mixr/include/mixr/base/units/Frequencies.hpp) | `GigaHertz` | Unidade de frequência: gigahertz, equivalente a Hertz × 0,000000001. |
| [`TeraHertz`](../contexts/src/mixr/include/mixr/base/units/Frequencies.hpp) | `TeraHertz` | Unidade de frequência: terahertz, equivalente a Hertz × 0,000000000001. |

### Massas

| Classe | Fábrica | Observação |
|---|---|---|
| [`Grams`](../contexts/src/mixr/include/mixr/base/units/Masses.hpp) | `Grams` | Unidade de massa: gramas, equivalente a Quilogramas × 1000. |
| [`KiloGrams`](../contexts/src/mixr/include/mixr/base/units/Masses.hpp) | `KiloGrams` | Unidade base de massa: quilogramas; uma instância com valor 1.0 é a unidade base. |
| [`Slugs`](../contexts/src/mixr/include/mixr/base/units/Masses.hpp) | `Slugs` | Unidade de massa: slug, equivalente a Quilogramas × 0,06852176585. |

### Potências

| Classe | Fábrica | Observação |
|---|---|---|
| [`KiloWatts`](../contexts/src/mixr/include/mixr/base/units/Powers.hpp) | `KiloWatts` | Unidade de potência: quilowatts, equivalente a Watts × 0,001. |
| [`Watts`](../contexts/src/mixr/include/mixr/base/units/Powers.hpp) | `Watts` | Unidade base de potência: watts; uma instância com valor 1.0 é a unidade base. |
| [`MilliWatts`](../contexts/src/mixr/include/mixr/base/units/Powers.hpp) | `MilliWatts` | Unidade de potência: miliwatts. |
| [`Horsepower`](../contexts/src/mixr/include/mixr/base/units/Powers.hpp) | `Horsepower` | Unidade de potência: horse-power (HP). |
| [`DecibelWatts`](../contexts/src/mixr/include/mixr/base/units/Powers.hpp) | `DecibelWatts` | Unidade de potência em decibéis: `10·log10(Watts)`. |
| [`DecibelMilliWatts`](../contexts/src/mixr/include/mixr/base/units/Powers.hpp) | `DecibelMilliWatts` | Unidade de potência em decibéis: `10·log10(Watts × 1000)`, equivalente a dBm. |

### Tempo

| Classe | Fábrica | Observação |
|---|---|---|
| [`Seconds`](../contexts/src/mixr/include/mixr/base/units/Times.hpp) | `Seconds` | Unidade base de tempo: segundos; uma instância com valor 1.0 é a unidade base. |
| [`MilliSeconds`](../contexts/src/mixr/include/mixr/base/units/Times.hpp) | `MilliSeconds` | Unidade de tempo: milissegundos, equivalente a Segundos / 1000,0. |
| [`MicroSeconds`](../contexts/src/mixr/include/mixr/base/units/Times.hpp) | `MicroSeconds` | Unidade de tempo: microssegundos, equivalente a Segundos / 1.000.000,0. |
| [`NanoSeconds`](../contexts/src/mixr/include/mixr/base/units/Times.hpp) | `NanoSeconds` | Unidade de tempo: nanossegundos, equivalente a Segundos / 1.000.000.000,0. |
| [`Minutes`](../contexts/src/mixr/include/mixr/base/units/Times.hpp) | `Minutes` | Unidade de tempo: minutos, equivalente a Segundos × 60,0. |
| [`Hours`](../contexts/src/mixr/include/mixr/base/units/Times.hpp) | `Hours` | Unidade de tempo: horas, equivalente a Segundos × 3600,0. |
| [`Days`](../contexts/src/mixr/include/mixr/base/units/Times.hpp) | `Days` | Unidade de tempo: dias, equivalente a Segundos × 3600,0 × 24,0. |

### Velocidades

| Classe | Fábrica | Observação |
|---|---|---|
| [`AngularVelocity`](../contexts/src/mixr/include/mixr/base/units/AngularVelocity.hpp) | `AngularVelocity` | Velocidade angular com unidade interna em radianos/segundo; converte a partir dos slots `angle` e `time`. |
| [`LinearVelocity`](../contexts/src/mixr/include/mixr/base/units/LinearVelocity.hpp) | `LinearVelocity` | Velocidade linear com unidade interna em metros/segundo; converte a partir dos slots `distance` e `time`. |

### Cores

| Classe | Fábrica | Observação |
|---|---|---|
| [`Color`](../contexts/src/mixr/include/mixr/base/colors/Color.hpp) | `Color` | Classe de propósito geral para cores, usada como base para as representações RGB, HSV e afins. |
| [`Cie`](../contexts/src/mixr/include/mixr/base/colors/Cie.hpp) | `cie` | Define uma cor pelo modelo CIE: luminância, coordenadas x/y e dados específicos de monitor. |
| [`Cmy`](../contexts/src/mixr/include/mixr/base/colors/Cmy.hpp) | `cmy` | Define uma cor pelos componentes Ciano, Magenta e Amarelo (CMY). |
| [`Hls`](../contexts/src/mixr/include/mixr/base/colors/Hls.hpp) | `hls` | Define uma cor por Matiz, Saturação e Luminosidade (Hue, Lightness, Saturation). |
| [`Hsv`](../contexts/src/mixr/include/mixr/base/colors/Hsv.hpp) | `hsv` | Define uma cor por Matiz, Saturação e Valor (Hue, Saturation, Value). |
| [`Hsva`](../contexts/src/mixr/include/mixr/base/colors/Hsva.hpp) | `hsva` | Define uma cor por Matiz, Saturação, Valor e Alfa (HSV acrescido de canal de transparência). |
| [`Rgb`](../contexts/src/mixr/include/mixr/base/colors/Rgb.hpp) | `rgb` | Define uma cor pelos componentes Vermelho, Verde e Azul (RGB). |
| [`Rgba`](../contexts/src/mixr/include/mixr/base/colors/Rgba.hpp) | `rgba` | Define uma cor por Vermelho, Verde, Azul e Alfa (RGB acrescido de canal de transparência). |
| [`Yiq`](../contexts/src/mixr/include/mixr/base/colors/Yiq.hpp) | `yiq` | Define uma cor no espaço YIQ usado no padrão NTSC: luminância Y e crominância I/Q. |

### Handlers de rede

| Classe | Fábrica | Observação |
|---|---|---|
| [`TcpClient`](../contexts/src/mixr/include/mixr/base/network/TcpClient.hpp) | `TcpClient` | Handler de rede: lado cliente de uma única conexão TCP/IP, conecta a um host/porta de destino. |
| [`TcpServerSingle`](../contexts/src/mixr/include/mixr/base/network/TcpServerSingle.hpp) | `TcpServerSingle` | Handler de rede: lado servidor de uma única conexão TCP/IP (aceita apenas uma conexão). |
| [`TcpServerMultiple`](../contexts/src/mixr/include/mixr/base/network/TcpServerMultiple.hpp) | `TcpServerMultiple` | Handler de rede: lado servidor TCP/IP que aceita múltiplas conexões, devolvendo um `TcpHandler` por conexão. |
| [`UdpBroadcastHandler`](../contexts/src/mixr/include/mixr/base/network/UdpBroadcastHandler.hpp) | `UdpBroadcastHandler` | Handler de rede: envio/recebimento de pacotes UDP em modo broadcast. |
| [`UdpMulticastHandler`](../contexts/src/mixr/include/mixr/base/network/UdpMulticastHandler.hpp) | `UdpMulticastHandler` | Handler de rede: envio/recebimento de pacotes UDP em modo multicast (endereço de grupo 224–239.x.x.x). |
| [`UdpUnicastHandler`](../contexts/src/mixr/include/mixr/base/network/UdpUnicastHandler.hpp) | `UdpUnicastHandler` | Handler de rede: envio de pacotes UDP unicast para um IP/porta de destino específico. |

### EarthModel

| Classe | Fábrica | Observação |
|---|---|---|
| [`EarthModel`](../contexts/src/mixr/include/mixr/base/EarthModel.hpp) | `EarthModel` | Modelo de elipsoide/geodesia da Terra: eixos maior/menor e achatamento, com vários modelos pré-definidos (ex.: WGS84). |

## Classes abstratas de extensão

`Player::updateSystemPointers()` resolve por **tipo** (não por nome de slot) os ~10 papéis
primários de um `Player` — `findByType()` acha o primeiro componente de `components:` cuja cadeia
de herança bate com o tipo procurado (ver CLAUDE.md, seção do `built-in_mixr_1`).

Sete desses papéis (`Pilot`, `Navigation`, `Datalink`, `Radio`, `Gimbal`, `RfSensor`,
`OnboardComputer`) já são classes concretas dentro de `mixr::models` (tabelas acima) — usáveis
direto ou como base para uma especialização própria; não duplicadas aqui. `StoresMgr` (o nome de
fábrica) resolve para a classe concreta `SimpleStoresMgr` — a base abstrata real (também chamada
`StoresMgr` no C++, fábrica `BaseStoresMgr`) é uma das classes órfãs (ver acima), não alcançável
por EDL.

Só **`DynamicsModel`** e **`IrSystem`** são genuinamente abstratas, sem contraparte concreta de
mesmo nome de fábrica — os pontos de extensão de verdade: para um novo tipo de dinâmica ou de
sistema IR, herda-se diretamente delas (uma herança, sem outro passo).

| Classe | Papel resolvido | Observação |
|---|---|---|
| [`DynamicsModel`](../contexts/src/mixr/include/mixr/models/dynamics/DynamicsModel.hpp) | `dynamicsModel` | Classe base abstrata de dinâmica de voo/movimento (herda de `base::Component`); sem branch em `factory.cpp` — ponto de extensão oficial do papel `dynamicsModel`. `AerodynamicsModel` e `SpaceDynamicsModel` são subclasses abstratas intermediárias (também sem branch próprio); `RacModel`, `LaeroModel` e `JSBSimModel` derivam de `AerodynamicsModel` e são as três implementações concretas construíveis via EDL, cada uma com uma abordagem de dinâmica diferente. |
| [`IrSystem`](../contexts/src/mixr/include/mixr/models/system/IrSystem.hpp) | `irSystem` | Classe base abstrata para sistemas infravermelhos (herda de `System`); sem branch em `factory.cpp` — ponto de extensão oficial do papel `irSystem`. `IrSensor` deriva diretamente dela e é construível via EDL; `MergingIrSensor` deriva de `IrSensor` (também com branch própria), mas é um beco sem saída neste fork por depender de `AirAngleOnlyTrkMgrPT`, uma classe órfã não construível pela factory. |

## Eventos nativos do MIXR

Além de classes, o MIXR já traz dois **vocabulários de eventos** prontos para uso — nenhuma linha
de C++ nova é necessária para enviá-los ou reagir a eles, o próprio framework já os dispara e/ou já
os trata em vários pontos. São **dois sistemas totalmente distintos** (macro diferente, header
diferente, função de despacho diferente, faixa numérica própria) — nunca confundir um pelo outro,
apesar do nome "evento" servir para os dois.

### `mixr::base::Component` — eventos (`event()`/`ON_EVENT`)

Cada token é um membro do `enum` dentro da própria classe `Component`
([`eventTokens.hpp`](../contexts/src/mixr/include/mixr/base/eventTokens.hpp), incluído
textualmente dentro do corpo de `Component.hpp` — por isso o código deste repositório escreve
`mixr::base::Component::RESET_EVENT`, não um namespace à parte). Despachado por
`component->event(id, obj)`, implementado pelas macros `BEGIN_EVENT_HANDLER`/`ON_EVENT`/
`ON_EVENT_OBJ`/`END_EVENT_HANDLER`
([`macros.hpp:327-361`](../contexts/src/mixr/include/mixr/base/macros.hpp)): primeiro `ON_EVENT`/
`ON_EVENT_OBJ` que casar o token vence; `ON_EVENT_OBJ` além disso exige que o payload passe num
`dynamic_cast` para o tipo esperado. Token não tratado cai para `BaseClass::event()` — **sobe a
hierarquia de classe**, não a árvore de componentes (só eventos de tecla, `≤ MAX_KEY_EVENT`, sobem
para `container()`, mecanismo que não se aplica a nenhum token desta seção).

Há uma cópia paralela, [`eventTokens.epp`](../contexts/src/mixr/include/mixr/base/eventTokens.epp)
(mesmos valores, só `#define EID_*`, para o parser EDL enxergar os tokens em arquivo de
configuração) — **com uma divergência real confirmada, vendorizada assim desde a v1.0.5**:
`EID_UPDATE_VALUE1` deveria se chamar `EID_UPDATE_VALUE` (grafia errada, evento de UI gráfica —
fora do escopo desta tabela de qualquer forma), e `REFUEL_EVENT` (1323) simplesmente não tem
`#define` nenhum no `.epp` — inacessível por nome a partir de um `.edl`, só por C++ direto.

**Fora de escopo aqui, deliberadamente**: a faixa de teclas (1–999, ~40 tokens tipo `CLR_KEY`/
`F1_KEY`..`F12_KEY`/`OSB_T1`..`T10`, herança de GLUT/widgets) e os eventos gráficos
(1201–1235, `UPDATE_INSTRUMENTS`/`SET_COLOR`/`SET_POSITION`/...) — nenhum dos dois faz sentido sem
`mixr_graphics`/`instruments`/`ighost`, que este fork **headless** não publica (mesmo motivo já
registrado no resto deste repositório para excluir essas árvores). Pelo mesmo motivo, das ~20
switches HOTAS que o MIXR define, só as 5 com handler real no fonte vendorizado entram na tabela
abaixo — as ~15 restantes (`TMS_*`/`DMS_*`/`CMS_*`/`PINKY_SW_EVENT`/`NWS_SW_EVENT`/`CURSOR_*`)
estão definidas em `eventTokens.hpp` mas nenhuma classe do fork as trata (reservadas para código de
cockpit/gráficos deste mesmo pacote ausente).

**Coluna Payload**: o tipo C++ exigido pelo `ON_EVENT_OBJ(token, handler, Tipo)` correspondente —
`event(id, obj)` só invoca esse handler se `dynamic_cast<Tipo*>(obj)` for bem-sucedido (ver a
mecânica de despacho acima). "—" significa que só existe `ON_EVENT` (sem objeto) para esse token
neste fork; token com handler registrado só via `ON_EVENT` nunca lê `obj`, mesmo que outro código
o envie com um.

#### Eventos de base

| Token | Valor | Payload | Observação |
|---|---|---|---|
| `SHUTDOWN_EVENT` | 1001 | — | Notificação de encerramento. Handler padrão: `Component::shutdownNotification()` propaga o mesmo evento recursivamente a todo subcomponente e marca `shutdown=true`; sobrescrito por ~20 classes (`Station`, `Simulation`, `AbstractPlayer`, `DataRecorder`, `NetIO`/`Nib` do DIS...). Este repositório dispara explicitamente em `app/src/app/Shutdown.cpp`, logo antes do `unref()` da `Station` — ver CLAUDE.md, seção "Décima oitava passada". |

#### Eventos de simulação

| Token | Valor | Payload | Observação |
|---|---|---|---|
| `RESET_EVENT` | 1301 | — | Handler padrão: `Component::onEventReset()` chama o `reset()` virtual — a "cadeia de reset" que ~40 classes sobrescrevem (`Station`, `Simulation`, `WorldModel`, `Player`, `Terrain`, `DataRecorder`...). Este repositório dispara uma vez, em `app/src/app/StationBuilder.cpp`, logo após montar a `Station`. |
| `FREEZE_EVENT` | 1302 | [`Number`](../contexts/src/mixr/include/mixr/base/numeric/Number.hpp) | Estado de freeze pedido (na prática, um `Boolean`, que deriva de `Number`). Handler padrão: `Component::event()` liga/desliga o próprio flag de freeze via `setSlotFreeze()` — nenhuma subclasse sobrescreve, é puramente um comportamento de `Component`. |
| `FREEZE_EVENT_ALL` | 1303 | — | Comentário do framework promete o mesmo payload de `FREEZE_EVENT`, mas **sem handler padrão no fonte vendorizado** — nenhum `ON_EVENT`/`ON_EVENT_OBJ` para este token em `contexts/src/mixr/src/`, logo nenhum `Tipo` chega a ser exigido de fato. |
| `KILL_EVENT` | 1304 | [`Player`](../contexts/src/mixr/include/mixr/models/player/Player.hpp) | O player que matou (opcional — `ON_EVENT(KILL_EVENT, killedNotification)` também existe, sem payload). Handler padrão: `Player::killedNotification()` propaga `KILL_EVENT` aos subcomponentes, marca dano/fumaça/chamas em 1.0, `setMode(KILLED)`, grava `REID_PLAYER_KILLED`; `System::killedNotification()` também existe (base mínima, sobrescrita por `Radar`/`Rwr`/`TrackManager`). |
| `CRASH_EVENT` | 1305 | [`Player`](../contexts/src/mixr/include/mixr/models/player/Player.hpp) | O player colidido (opcional — `ON_EVENT(CRASH_EVENT, crashNotification)`, sem payload, é o caso "colidiu com o terreno"). Handler padrão: `Player::crashNotification()` (sem payload) faz `setMode(CRASHED)` e propaga `KILL_EVENT` aos subcomponentes, grava `REID_PLAYER_CRASH`; `Player::collisionNotification(Player*)` (com payload) faz o mesmo e grava `REID_PLAYER_COLLISION`. Ambos sobrescritos por `AbstractWeapon` (detona no impacto) e `Effect`. |
| `JETTISON_EVENT` | 1306 | [`AbstractWeapon`](../contexts/src/mixr/include/mixr/models/player/weapon/AbstractWeapon.hpp) ou [`ExternalStore`](../contexts/src/mixr/include/mixr/models/system/ExternalStore.hpp) | `Stores::event()` registra `ON_EVENT_OBJ` para os dois tipos separadamente (`onJettisonEvent(AbstractWeapon*)`/`onJettisonEvent(ExternalStore*)`); `AbstractWeapon::event()` só tem `ON_EVENT` (sem payload) para o próprio `onJettisonEvent()`. `Stores` também é emissor — dispara para o par arma/pilone ao ejetar. |
| `RF_EMISSION` | 1307 | [`Emission`](../contexts/src/mixr/include/mixr/models/Emission.hpp) | Handler padrão em `Gimbal`/`Player` (`onRfEmissionEvent(Emission*)`) — roteia uma emissão de RF recebida para o modelo de sensor/player. |
| `RF_EMISSION_RETURN` | 1308 | [`Emission`](../contexts/src/mixr/include/mixr/models/Emission.hpp) | Handler padrão em `Antenna` (`onRfEmissionReturnEventAntenna(Emission*)`). |
| `DESIGNATOR_EVENT` | 1309 | [`Designator`](../contexts/src/mixr/include/mixr/models/Designator.hpp) | Handler padrão em `AbstractWeapon` (`onDesignatorEvent(Designator*)`). |
| `DATALINK_MESSAGE` | 1310 | [`Object`](../contexts/src/mixr/include/mixr/base/Object.hpp) | Tipo genérico — `Datalink`/`Player` aceitam qualquer `base::Object*` como mensagem de datalink, sem uma classe de mensagem própria. Handler padrão em `Datalink`/`Player` (`onDatalinkMessageEvent[Player]()`). |
| `ON_OWNSHIP_CONNECT` | 1311 | — | Ao novo player "ownship" (o comentário do token sugere `Player`, mas sem `ON_EVENT_OBJ` registrado o tipo nunca é exigido). Definido, mas **sem handler padrão no fonte vendorizado**. |
| `ON_OWNSHIP_DISCONNECT` | 1312 | — | Ao antigo player "ownship", na desconexão — mesma observação de `ON_OWNSHIP_CONNECT`. Definido, mas **sem handler padrão no fonte vendorizado**. |
| `SCAN_START` | 1313 | [`Integer`](../contexts/src/mixr/include/mixr/base/numeric/Integer.hpp) | Handler padrão em `RfSensor`/`ScanGimbal` (`onStartScanEvent(Integer*)`). |
| `SCAN_END` | 1314 | [`Integer`](../contexts/src/mixr/include/mixr/base/numeric/Integer.hpp) | Handler padrão em `RfSensor`/`ScanGimbal` (`onEndScanEvent(Integer*)`). |
| `WPN_RELOAD` | 1315 | — | Handler padrão em `StoresMgr` (`onWpnReload()`). |
| `RF_REFLECTED_EMISSION` | 1316 | [`Emission`](../contexts/src/mixr/include/mixr/models/Emission.hpp) | Handler padrão em `Player` (`onRfReflectedEmissionEventPlayer(Emission*)`). |
| `RF_REFLECTIONS_REQUEST` | 1317 | [`Component`](../contexts/src/mixr/include/mixr/base/Component.hpp) | Tipo genérico — aceita qualquer `base::Component*` (tipicamente quem está pedindo a reflexão). Handler padrão em `Player` (`onReflectionsRequest(base::Component*)`). |
| `RF_REFLECTIONS_CANCEL` | 1318 | [`Component`](../contexts/src/mixr/include/mixr/base/Component.hpp) | Tipo genérico, mesma observação de `RF_REFLECTIONS_REQUEST`. Handler padrão em `Player` (`onReflectionsCancel(base::Component*)`). |
| `IR_QUERY` | 1319 | [`IrQueryMsg`](../contexts/src/mixr/include/mixr/models/IrQueryMsg.hpp) | Handler padrão em `Player` (`onIrMsgEventPlayer(IrQueryMsg*)`). |
| `IR_QUERY_RETURN` | 1320 | [`IrQueryMsg`](../contexts/src/mixr/include/mixr/models/IrQueryMsg.hpp) | Handler padrão em `IrSeeker` (`irQueryReturnEvent(IrQueryMsg*)`). |
| `SAT_COMM_MSG` | 1321 | — | Definido, mas **sem handler padrão no fonte vendorizado**. |
| `DE_EMISSION` | 1322 | [`Object`](../contexts/src/mixr/include/mixr/base/Object.hpp) | Tipo genérico — sem uma classe de mensagem própria para energia dirigida. Handler padrão em `Player` (`onDeEmissionEvent(base::Object*)`). |
| `REFUEL_EVENT` | 1323 | — | Definido, mas **sem handler padrão no fonte vendorizado** — e é o token ausente do `eventTokens.epp` citado acima, então nem chega a ser alcançável por nome a partir de um `.edl`. |

#### HOTAS com handler padrão

Das switches HOTAS que o MIXR define (faixa 1400-1423), só estas 5 têm handler real no fonte
vendorizado — as demais estão listadas na nota de exclusão acima.

| Token | Valor | Payload | Observação |
|---|---|---|---|
| `SENSOR_RTS` | 1400 | — | Sensor — retorno à busca. Handler padrão em `RfSensor` (`onReturnToSearchEvent()`). |
| `TGT_STEP_EVENT` | 1401 | — | Alvo, passo/rejeição. Handler padrão em `Player`. |
| `TGT_DESIGNATE` | 1402 | — | Alvo, designação. Handler padrão em `RfSensor`. |
| `WPN_REL_EVENT` | 1405 | [`Boolean`](../contexts/src/mixr/include/mixr/base/numeric/Boolean.hpp) | Liberação de arma; estado do switch (opcional — `ON_EVENT` sem payload também existe, para disparo único). Handler padrão em `Player`/`StoresMgr`. |
| `TRIGGER_SW_EVENT` | 1406 | [`Boolean`](../contexts/src/mixr/include/mixr/base/numeric/Boolean.hpp) | Gatilho; mesma semântica de `WPN_REL_EVENT`. Handler padrão em `Player`/`StoresMgr`. |

### `mixr::simulation` — eventos do Data Recorder (`REID_*`)

Sistema **separado** do de `Component` acima: os tokens `REID_*` são definidos em
[`dataRecorderTokens.hpp`](../contexts/src/mixr/include/mixr/simulation/dataRecorderTokens.hpp)
(também só `#define`, mesma razão do `.epp`), despachados por
`AbstractDataRecorder::recordData(id, obj[4], val[4])` via as macros `BEGIN_RECORD_DATA_SAMPLE`/
`SAMPLE_n_OBJECT(S)`/`SAMPLE_n_VALUE(S)` do lado de quem emite e `BEGIN_RECORDER_HANDLER_TABLE`/
`ON_RECORDER_EVENT_ID`/`END_RECORDER_HANDLER_TABLE` do lado do `DataRecorder` que recebe — nenhuma
relação de macro, header ou função de despacho com `event()`/`ON_EVENT`. Token não tratado vira
`REID_UNHANDLED_ID_TOKEN` (ver `libs/xlog`/`libs/xmsg` no CLAUDE.md para o porquê deste
repositório não usa o `mixr::recorder` para tudo — o schema é fechado, sem campo de texto livre).

**Único ponto de contato entre os dois sistemas**: `RESET_EVENT` (o de `Component`, acima) ao
chegar em `DataRecorder::reset()` (via `onEventReset()` → `reset()` virtual, cadeia comum a toda
classe) emite um `REID_RESET_EVENT` na gravação — não são o mesmo enum, só esse um cruzamento.

**Sem coluna Payload aqui, de propósito**: ao contrário de `ON_EVENT_OBJ`, a assinatura de
`recordData()` é `base::Object* pObjects[4]` — um array genérico, sem `Tipo` amarrado ao token pelo
compilador. O que cada `P1`..`P4` da tabela abaixo carrega (ex.: "P1⇒player") é documentado só em
comentário no header, não reforçado por `dynamic_cast` nenhum — informação real, mas de uma
natureza diferente da coluna Payload da seção anterior; por isso já vem embutida na própria
Observação de cada linha, em vez de uma coluna à parte.

| Token | Valor | Observação |
|---|---|---|
| `REID_END_OF_DATA` | 0 | Última mensagem de um arquivo de gravação — a única que não pode ser desabilitada. |
| `REID_FILE_ID` | 1 | Identificador de arquivo (cabeçalho), gravado uma vez no início do fluxo. |
| `REID_UNHANDLED_ID_TOKEN` | 2 | Token desconhecido/não tratado — `DataRecorder::processUnhandledId()`, o "fallback" de todo token sem handler. |
| `REID_RESET_EVENT` | 3 | Evento de reset da simulação. Handler padrão: `DataRecorder::reset()` — ver a ponte com `RESET_EVENT` acima. |
| `REID_MARKER` | 21 | Marcador de dado; V1⇒id, V2⇒id de origem. Handler padrão: `recordMarker`. |
| `REID_DI_EVENT` | 22 | Entrada discreta (switch etc.); V1⇒id, V2⇒origem, V3⇒valor. Handler padrão: `recordDI`. |
| `REID_AI_EVENT` | 23 | Entrada analógica (joystick etc.); V1⇒id, V2⇒origem, V3⇒valor. Handler padrão: `recordAI`. |
| `REID_NEW_PLAYER` | 41 | Novo player; P1⇒player. Handler padrão: `recordNewPlayer` — emitido por `Simulation`. |
| `REID_PLAYER_REMOVED` | 42 | Player removido; P1⇒player. Handler padrão: `recordPlayerRemoved` — emitido por `Simulation`. |
| `REID_PLAYER_DATA` | 43 | Dados de estado do player; P1⇒player. Handler padrão: `recordPlayerData` — emitido por `Player` (exige `dataLogTime` no slot, ver armadilha 1 de `libs/xtacview`). |
| `REID_PLAYER_DAMAGED` | 44 | Dano por detonação; P1⇒player, P2⇒arma. Handler padrão: `recordPlayerDamaged`. |
| `REID_PLAYER_COLLISION` | 45 | Colisão; P1⇒player, P2⇒outro player. Handler padrão: `recordPlayerCollision` — emitido por `Player::collisionNotification()`. |
| `REID_PLAYER_CRASH` | 46 | Queda; P1⇒player. Handler padrão: `recordPlayerCrash` — emitido por `Player::crashNotification()`. |
| `REID_PLAYER_KILLED` | 47 | Morte; P1⇒player, P2⇒atirador. Handler padrão: `recordPlayerKilled` — emitido por `Player::killedNotification()`. |
| `REID_WEAPON_RELEASED` | 61 | Arma liberada; P1⇒arma, P2⇒atirador, P3⇒alvo. Handler padrão: `recordWeaponReleased` — emitido por `AbstractWeapon`/`Missile`. |
| `REID_WEAPON_HUNG` | 62 | Arma emperrada (hung store); mesmos P1-P3. Handler padrão: `recordWeaponHung`. |
| `REID_WEAPON_DETONATION` | 63 | Detonação; mesmos P1-P3, V1⇒tipo de detonação, V2⇒distância de erro. Handler padrão: `recordWeaponDetonation`. |
| `REID_GUN_FIRED` | 64 | Disparo de canhão; P1⇒atirador, v[0]⇒número de rounds. Handler padrão: `recordGunFired` — emitido por `Guns`. |
| `REID_NEW_TRACK` | 81 | Nova pista; P1⇒player, P2⇒pista. Handler padrão: `recordNewTrack` — emitido por todo `*TrkMgr`, mas ⚠ armadilha já documentada na seção `libs/xtacview` deste repositório: **na prática nunca chega**, degrada para `REID_UNHANDLED_ID_TOKEN` (use `REID_TRACK_DATA` e deduza o primeiro contato pela primeira amostra de cada `track_id`). |
| `REID_TRACK_REMOVED` | 82 | Pista removida; P1⇒player, P2⇒pista. Handler padrão: `recordTrackRemoved`. |
| `REID_TRACK_DATA` | 83 | Dados de pista; P1⇒player, P2⇒pista. Handler padrão: `recordTrackData`. |

Duas faixas reservadas, sem token nomeado individual: **500–999** (uso interno reservado) e
**1000–9999** (eventos definidos pelo usuário — é essa faixa que os tokens de usuário deste
repositório, se algum dia precisar, devem ocupar).

## Ler também

- [`../CLAUDE.md`](../CLAUDE.md), seção "O modelo MIXR em uma tela" — como a cadeia de factories
  nativas se encaixa (`local → xtacview → simulation → models → recorder → base`) e por que a
  primeira que retorna não-nulo vence
- [`REGISTRO.md`](REGISTRO.md) — quem já está mexendo em qual modelo deste repositório (os
  próprios, não os nativos catalogados aqui)
- [`../CONTRIBUTING.md`](../CONTRIBUTING.md) — o roteiro de contribuir com um modelo novo
- [`../src/poc/built-in_mixr_1/README.md`](../src/poc/built-in_mixr_1/README.md) — o inventário
  parcial (53 das 96 classes de `mixr::models`, as que cabem nos ~10 papéis primários de um único
  `Player`) que este documento generaliza para as 225 classes nativas construíveis do fork inteiro
