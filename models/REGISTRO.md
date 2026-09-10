# `models/REGISTRO.md` — quem já existe, quem cuida

Coordenação entre devs, leitura humana — não é usado por nenhum alvo de build/teste/guard (a
descoberta de modelos continua via `find`). Existe para responder, num relance, "quem já está
mexendo em qual modelo".

Atualização é manual, sem enforcement automatizado. Ao abrir um PR de modelo novo, acrescente sua
própria linha no **fim** da tabela; ao mudar algo, edite só a sua linha, nunca a de outro modelo.

| Modelo | Pasta | Responsável | Última atualização | Observação |
|---|---|---|---|---|
| A-4 (flight) | `models/players/A-4/` | — | — | produção — `flight`, `bandit`, `python-flight`, `onnx-policy`, `built-in_mixr_1` |
| C-130 | `models/players/C-130/` | — | 2026-09-10 | produção — navegação nativa (Route/Steerpoint) + liberação de paraquedista, agora em DUAS formas: `C130ActionParatrooperRelease` (uma estação por cruzamento, placeholder `Effect`) e `C130ActionParatrooperStick` (várias em sequência, com intervalo) — poc `c130-airdrop`, `sandbox/C-130-6DOF`, `sandbox/C-130_paratrooper-6DOF` (a integração de verdade com `models/players/paratrooper`, 30 `( Paratrooper )` reais liberados via stick). Rumo/altitude/velocidade sustentam (medido, 600s sem crash) — motor/thruster vendorizados incompatíveis corrigidos (`t56.xml` + thruster `direct`), ver `c130ap.xml` |
| template | `models/template/` | — | — | ponto de partida, não é modelo de produção — não reivindicar; também hospeda o mirror de contrato (`src/mirror.cpp`) usado pelos testes de plugin do host |
| paratrooper | `models/players/paratrooper/` | — | 2026-09-10 | produção — corpo físico + decisão de um paraquedista (`Effect`-derivado, FSM de mão única FREEFALL→CANOPY→LANDED) — poc `paratrooper-drop`, autônoma. **Já ligado ao C-130** em `sandbox/C-130_paratrooper-6DOF` (30 liberados via `C130ActionParatrooperStick`, 1,5s de intervalo, cruzando plugins sem nenhuma mudança de C++ em nenhum dos dois). Determinismo confirmado 1/2/4 threads T/C, 6000 frames (poc autônoma) |
| Navstar-3 | `models/others/Navstar-3/` | — | 2026-09-10 | produção — primeiro modelo na categoria `others`. Satélite de navegação (GPS/NAVSTAR) em órbita circular, `( SpaceVehicle )` nativo sem `DynamicsModel` nenhum — a órbita é 100% calculada por `domain::CircularOrbit` (o MIXR não tem mecânica orbital nativa, confirmado no fonte) e aplicada via `setGeocPosition(slaved=true)`. Árvore decide `SUNLIT`/`ECLIPSE` (`domain::EclipseGeometry`, sombra cilíndrica, direção de sol fixa configurável). Sem classe de corpo físico própria (`SpaceVehicle` já é nativo) — 4 nomes de fábrica, todos prefixados `Navstar3*`. Poc `navstar3-orbit`, autônoma. Determinismo confirmado 1/2/4 threads T/C, 1200 frames. Efeito colateral: generalizou `src/ui/scripts/generate_edl_catalog.py` para descobrir modelo em qualquer categoria (`players`/`systems`/`others`), não só `players/` |


## Ler também

- [`../CONTRIBUTING.md`](../CONTRIBUTING.md) — o roteiro completo de contribuir com um modelo novo,
  inclusive registrar um cenário novo (seção 5) e decidir a cobertura de teste (seção 5.3)
- [`../CLAUDE.md`](../CLAUDE.md), seção "O MODELO é um plugin, construído numa etapa PRÉVIA" —
  visão geral de `models/` e o build em etapas do repositório inteiro
