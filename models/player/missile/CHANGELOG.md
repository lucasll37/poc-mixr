# Changelog — `missile`

Todo projeto de modelo deste repositório tem `tests/`, `docs/`, `README.md` e **este arquivo** —
a regra, e o porquê dela, estão em [../README.md](../README.md); a guarda
[`tests/guard/check_modelo_estrutura.sh`](../../tests/guard/check_modelo_estrutura.sh) a trava.

Formato adaptado de [Keep a Changelog](https://keepachangelog.com/pt-br/1.1.0/).

**A versão é a do `project()` em [meson.build](meson.build)** — hoje `1.0.0`. Não existe outra:
não há tag de git, e o descritor do plugin não carrega versão do modelo (`PluginDescV1` tem
`plugin_name`, `mixr_pkg_version` e `build_id`, e nada mais — ver
[`shared/xplugin/PluginAbi.hpp`](../../shared/xplugin/PluginAbi.hpp)).

**As datas saem da data de COMMIT, nunca da mensagem** — todo commit deste repositório se chama
`up`. As entradas anteriores à criação deste arquivo foram reconstruídas do código e dos
documentos: uma linha por mudança que alguém precisaria saber antes de mexer neste modelo, não
uma por commit.

---

## [Não versionado]

### Adicionado

- Este `CHANGELOG.md`. (2026-09-03)

### Mudado

- **`make install-host` deposita em `models/plugins/`, nunca mais em `dist/`** — o mesmo depósito
  que um `.so` de terceiro usa; quem sincroniza `models/plugins/` → `dist/lib/mixr-plugins/` é o
  alvo `sync-plugins` do `make install` da raiz. Compilar este modelo deixou de presumir onde o
  host guarda os artefatos dele. (2026-09-03)

---

## [1.0.0] — 2026-09-02

O modelo nasceu inteiro neste dia, como **segundo exemplo** de "criar um modelo novo" — cópia da
receita de `models/fixtures/stub` (ver [../README.md](../README.md) §2), e o primeiro modelo deste
repositório que publica um `Player`.

### Adicionado

- **`xmissile::GuidedMissile`** — um `Player` novo (deriva de `models::Missile`), guiado sobre um
  `JSBSimModel` anexado: física 6-DOF de verdade, não um `Player` de brinquedo. É o único nome que
  o plugin publica.
- **`domain::pursuit()`** (`src/domain/Guidance.{hpp,cpp}`) — a lei de guiagem, **pura** (sem
  MIXR): entra NED em metros e graus (a mesma convenção do `domain::WorldView` do `flight`), sai
  comando normalizado `-1..1`, pronto para `setControlStickRollInput()`/`PitchInput()`. É
  perseguição pura, não navegação proporcional — decisão deliberada, documentada em
  `Guidance.hpp`: para um alvo único, não manobrando, no alcance curto da demo, ela converge e é
  trivial de explicar. **Proporcional-derivativo, não só proporcional**: o termo de taxa é o
  amortecimento que falta a um controlador só-P, e `tests/domain` trava essa propriedade.
- **Plugin PRÓPRIO, ao lado do `flight`** — carregado por um segundo `( PluginModule )`, só no
  cenário de demo `src/poc/dis/single-thread/configs/scenario_missile_demo.edl.in`. O motivo é o
  `provides:`, que é igualdade EXATA de conjunto: acrescentar `GuidedMissile` aos nomes do
  `flight` obrigaria TODO cenário que carrega aquele `.so` — os de produção inclusive — a
  atualizar a lista.
- **Dependências: só `mixr_dep` + `sdk_dep`.** Não linka a BehaviorTree.CPP — este modelo não
  decide com árvore, só guia e voa. É a prova, junto com o `stub`, de que o contrato de plugin não
  impõe pilha interna nenhuma.
- As peças que faziam o projeto autocontido **à época** — o `CHANGELOG.md`, a quinta, é desta
  rodada: **`Makefile` autocontido**,
  **`tests/domain/`** (só `domain::pursuit()`, sem levantar `Station`), **`docs/DESIGN.md`** e
  **`README.md`**.

### Armadilhas registradas na estreia

- **Não reduzir `ixx`/`iyy`/`izz` do `aim1.xml`.** Reduzi-los na mesma proporção do peso, para
  "mais carga G", deixa o modo de arfagem/rolagem pouco amortecido demais para o passo fixo de
  0,02 s do JSBSim — medido divergindo (mais de 100° de banco/arfagem, velocidade em milhares de
  nós em menos de 0,3 s) mesmo com o comando limitado em taxa. Ficam os do c310, cujas tabelas
  aerodinâmicas foram calibradas para essa inércia; só `emptywt` cai (~15×), o que já basta.
- **O timer de destruição mora em `updateTC()`, não em `dynamics()`** — uma vez `DETONATED`,
  `Player::updateTC()` para de chamar `dynamics()` e um timer ali nunca avança. E `updateTC()` é
  chamado 4× por frame, uma por fase, com `dt = dt_do_frame/4`: o acumulador só soma numa fase
  fixa **e** multiplica por 4.
- **A definição JSBSim do míssil (`aim1.xml`) mora em `models/flight/data/jsbsim/aircraft/aim1/`**,
  não aqui — é a árvore que todo `rootDir:` dos cenários já aponta
  (`./dist/share/mixr-plugins/flight/jsbsim/`). Este projeto publica só código.

Ver `docs/DESIGN.md` para a guiagem em detalhe e `CLAUDE.md` (seção "Demo: míssil guiado") para o
resto: envelope de lançamento, nome de fábrica `StoresMgr`, e comentário XML sem `--`.
