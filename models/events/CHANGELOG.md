# Changelog — `events/`

Formato adaptado de [Keep a Changelog](https://keepachangelog.com/pt-br/1.1.0/) — com uma
diferença do `CHANGELOG.md` de cada projeto de modelo (`models/player/A4`, `models/player/missile`,
`models/player/fixtures/stub`): **não há número de versão aqui.** `events/` não é um projeto Meson
próprio — não tem `project()`, se junta ao `poc-mixr` da raiz via `subdir('./events')` — então não
existe um `project()` cuja versão datar as entradas. A unidade que importa aqui é o **evento**:
cada linha abaixo é um token/payload adicionado ou mudado, não um número de release.

**As datas saem da data de COMMIT, nunca da mensagem** — todo commit deste repositório se chama
`up`. Mesma convenção já usada nos `CHANGELOG.md` de `models/*` (ver, por exemplo,
[`models/player/missile/CHANGELOG.md`](../player/missile/CHANGELOG.md)).

Ver [README.md](README.md) para a convenção completa (o que é um evento, como despachar, por que
o payload mora numa `shared_library()`) e [EventTokens.hpp](EventTokens.hpp) para o registro de
tokens em código — este arquivo é a HISTÓRIA de como ele cresceu, não substitui nenhum dos dois.

---

## [Não versionado]

### Adicionado

- **`events::EID_ALERT` / `events::TacticalAlert`** ([payloads/EID_ALERT/TacticalAlert.hpp](payloads/EID_ALERT/TacticalAlert.hpp)) —
  primeiro evento da convenção, generalizando o `xnative::TacticalAlert` que já existia só dentro
  de `models/player/A4`. Ganhou uma segunda via de entrega (broadcast direto por `EID_ALERT`, além do
  `sendMessage()` nativo do `Datalink`) para alcançar um player **sem** Datalink —
  `xmissile::GuidedMissile` (`models/player/missile`) é o primeiro handler escrito num plugin diferente
  do que emite. Mesma classe, mesmo nome de fábrica `"TacticalAlert"` de antes — nenhuma mudança
  em `provides:` de nenhum cenário. (2026-09-03)
- Este `CHANGELOG.md` e o `README.md` da convenção. (2026-09-03)

### Mudado

- **`payloads/<TOKEN>/` — uma pasta por evento, nomeada igual ao token** (`payloads/EID_ALERT/`
  para `events::EID_ALERT`) — o payload deixou de ficar solto direto em `payloads/` assim que
  ficou claro que o nome da pasta devia bater com o nome do token (achar o payload de um token só
  pelo nome, sem grepar). `EventTokens.hpp` continua como ledger único, fora de `payloads/` — ver
  o "porquê" no cabeçalho dele e no `README.md`. (2026-09-03)
- **Pasta promovida de `shared/xevents/` para `events/`, na raiz** — exceção deliberada à
  convenção `shared/x<nome>/` das outras seis `shared_library()` de fronteira de plugin: evento é
  o eixo central da modelagem deste projeto, não um detalhe de infraestrutura como as outras
  seis. O mecanismo de publicação (SDK, `shared_library()`, `sdk_dep`) não mudou em nada — só o
  endereço do fonte. (2026-09-03)
