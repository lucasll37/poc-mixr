# `models/REGISTRO.md` — quem já existe, quem cuida

Coordenação entre devs, leitura humana — não é usado por nenhum alvo de build/teste/guard (a
descoberta de modelos continua via `find`). Existe para responder, num relance, "quem já está
mexendo em qual modelo" — desde que a coluna **Responsável** seja preenchida; hoje isso vale só
para as linhas marcadas como "produção" (preenchidas a partir do `git log` de cada pasta —
autor único até aqui). As linhas de exercício/template seguem com "—": ninguém assumiu
responsabilidade formal por elas, e não há enforcement nenhum que cobre isso — um PR pode
deixar sua própria linha sem preencher sem quebrar build/teste/guard nenhum.

Atualização é manual, sem enforcement automatizado. Ao abrir um PR de modelo novo, acrescente sua
própria linha no **fim** da tabela, com seu nome na coluna Responsável; ao mudar algo, edite só a
sua linha, nunca a de outro modelo.

| Modelo | Pasta | Responsável | Última atualização | Observação |
|---|---|---|---|---|
| A-4 (flight) | `models/players/A-4/` | Lucas | — | produção — `flight`, `bandit`, `python-flight`, `onnx-policy` |
| C-130 | `models/players/C-130/` | Lucas | 2026-09-10 | produção — navegação nativa (Route/Steerpoint) + liberação de paraquedista, agora em DUAS formas: `C130ActionParatrooperRelease` (uma estação por cruzamento, placeholder `Effect`) e `C130ActionParatrooperStick` (várias em sequência, com intervalo) — `sandbox/C-130-6DOF`, `sandbox/C-130_paratrooper-6DOF` (a integração de verdade com `models/players/paratrooper`, 30 `( Paratrooper )` reais liberados via stick). Rumo/altitude/velocidade sustentam (medido, 600s sem crash) — motor/thruster vendorizados incompatíveis corrigidos (`t56.xml` + thruster `direct`), ver `c130ap.xml` |
| template | `models/template/` | — | — | ponto de partida, não é modelo de produção — não reivindicar; também hospeda o mirror de contrato (`src/mirror.cpp`) usado pelos testes de plugin do host |
| paratrooper | `models/players/paratrooper/` | Lucas | 2026-09-10 | produção — corpo físico + decisão de um paraquedista (`Effect`-derivado, FSM de mão única FREEFALL→CANOPY→LANDED). **Ligado ao C-130** em `sandbox/C-130_paratrooper-6DOF` (30 liberados via `C130ActionParatrooperStick`, 1,5s de intervalo, cruzando plugins sem nenhuma mudança de C++ em nenhum dos dois). Determinismo confirmado 1/2/4 threads T/C, 6000 frames |
| Navstar-3 | `models/others/Navstar-3/` | Lucas | 2026-09-10 | produção — primeiro modelo na categoria `others`. Satélite de navegação (GPS/NAVSTAR) em órbita circular, `( SpaceVehicle )` nativo sem `DynamicsModel` nenhum — a órbita é 100% calculada por `domain::CircularOrbit` (o MIXR não tem mecânica orbital nativa, confirmado no fonte) e aplicada via `setGeocPosition(slaved=true)`. Árvore decide `SUNLIT`/`ECLIPSE` (`domain::EclipseGeometry`, sombra cilíndrica, direção de sol fixa configurável). Sem classe de corpo físico própria (`SpaceVehicle` já é nativo) — 4 nomes de fábrica, todos prefixados `Navstar3*`. Cenário: `sandbox/Navstar-3-constellation`. Determinismo confirmado 1/2/4 threads T/C, 1200 frames. Efeito colateral: generalizou `src/ui/scripts/generate_edl_catalog.py` para descobrir modelo em qualquer categoria (`players`/`systems`/`others`), não só `players/` |
| missile | `models/players/missile/` | — | 2026-09-10 | exercício — `( GuidedMissile )`, subclasse cinemática de `mixr::models::Missile` (sem `dynamicsModel`), guiagem por navegação proporcional de verdade + espoleta de proximidade em `domain/Guidance.hpp`, puras. Sem `bt/`/`ubf/` (guiagem é controle contínuo, não decisão discreta) e sem slot novo (reusa `maxSpeed`/`maxg`/`lethalRange`/`maxBurstRng` herdados). Cenário: `sandbox/A4-6DOF-MISSILE` (um A-4 detecta outro pelo radar e dispara — a extensão de lançamento mora em `models/players/A-4`, não aqui) |
| AAA | `models/players/AAA/` | — | 2026-09-11 | exercício — antiaérea estacionária com "domo" de alcance (`( AaaSite )`, subclasse de `mixr::models::SamVehicle`, herda os slots nativos `minLaunchRange`/`maxLaunchRange`) que dispara o MESMO `( GuidedMissile )` (`models/players/missile`, sem nenhuma modificação) contra um alvo hostil que entra no domo. Ao contrário do `missile`, usa a pilha BT/UBF COMPLETA (`domain/`→`bt/`→`ubf/`→`xnative/`, árvore de 2 ramos) — decisão confirmada com o usuário, para espelhar a arquitetura do A-4 mesmo numa regra única. `( UbfAgent )` nativo hospeda o agente, sem nenhuma subclasse de `AgentTC` (decide à taxa de fundo, 10 Hz, suficiente para um alvo terrestre). Armadilha documentada: `SamVehicle::isLauncherReady()` nunca funciona com `GuidedMissile` (conta munição via `dynamic_cast<const Sam*>`, e as duas classes são irmãs, não uma filha da outra) — usa `StoresMgr::available()` direto. A extensão de RWR/evasão do lado do A-4 mora em `models/players/A-4`, não aqui. Cenário: `sandbox/AAA-A4-6DOF` |
| Beacon | `models/players/Beacon/` | Lucas | 2026-09-13 | exercício — modelo de EVENTO, não de decisão: `( Beacon )` herda `mixr::models::Player` direto (mesmo padrão mínimo de `mixr::models::Building`), sem `dynamicsModel`/`pilot`/sensor. Emite `events::EID_PING`/`events::PingMessage` (payload novo, não-nulo, em `models/events/payloads/EID_PING/`) periodicamente na fase de fundo (`updateData()`, sequencial — por isso sem mutex, ao contrário de `AlertDatalink` que decide na fase 3 em paralelo) e TRATA o mesmo evento na MESMA classe — o primeiro caso deste repositório em que emissor e receptor coincidem. Sem `domain/`/`bt/`/`ubf/`/`xnative/` separados: só `include/Beacon.hpp` + `src/Beacon.cpp` + `src/plugin.cpp` (factory inline, sem árvore de comportamento nenhuma). Cenário: `src/poc/my-event/` |


## Ler também

- [`../CONTRIBUTING.md`](../CONTRIBUTING.md) — o roteiro completo de contribuir com um modelo novo,
  inclusive registrar um cenário novo (seção 5) e decidir a cobertura de teste (seção 5.4)
- [`../CLAUDE.md`](../CLAUDE.md), seção "O MODELO é um plugin, construído numa etapa PRÉVIA" —
  visão geral de `models/` e o build em etapas do repositório inteiro
