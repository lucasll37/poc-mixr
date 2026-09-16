# `Beacon` — arquitetura

## O problema que este modelo existe para exercitar

`models/events/README.md` documenta a convenção de evento do MIXR (token + payload + emissor +
receptor), com um único caso de referência até aqui: `events::EID_ALERT`/`events::TacticalAlert`,
emitido por `xnative::AlertDatalink::broadcastAlert()` e tratado por
`xnative::AlertDatalink::onDatalinkMessageEvent()` (`models/players/air/A-4`) — duas classes
DIFERENTES, uma emitindo, outra tratando (por caminhos diferentes, inclusive: `sendMessage()`
nativo do `Datalink` e broadcast direto).

Faltava um caso mostrando o outro extremo: uma classe que emite **e** trata o **mesmo** evento,
sem depender de nenhum subsistema nativo por trás. `Beacon` é esse caso — construído para ser lido,
não para ser útil na simulação em si.

## Por que herdar `Player` direto, e não `Aircraft`/algo com dinâmica

O propósito é o evento, não voo. Qualquer coisa além do mínimo (`dynamicsModel`, `pilot`, sensor)
seria peso morto que não ajuda a explicar o mecanismo — e pior, competiria visualmente com o que
importa. `mixr::models::Building` já é o precedente nativo deste padrão: `Player` direto, só
`getMajorType()` sobrescrito. `Beacon` segue a mesma receita, com `BUILDING` como tipo (um "farol"
parado é uma metáfora razoável de instalação fixa).

## Por que emitir na fase de FUNDO (`updateData()`), não na fase 3 (tempo crítico)

`xnative::AlertDatalink::broadcastAlert()` (a referência existente) é chamado de dentro de
`FlightAction::execute()` — a decisão de um agente UBF, na fase 3 do frame de tempo crítico, **um
player por thread do pool**. Por isso ela precisa de um `std::mutex` (`alertMutex`): dois emissores
podem estar escrevendo no MESMO receptor ao mesmo tempo, em threads diferentes.

`Beacon` não tem agente nenhum — não há UBF, não há árvore, não há razão para decidir na fase 3.
`updateData()` (a fase de fundo, 10 Hz em tempo real) é chamada
`Simulation::updateBgPlayerList()`, **sequencialmente, numa única thread**, um player de cada vez
(confirmado lendo o fonte antes de escrever qualquer código — `contexts/src/mixr/src/simulation/
Simulation.cpp`, `updateData()`/`updateBgPlayerList()`). Como resultado, `Beacon::broadcastPing()`/
`Beacon::onPingEvent()` **não precisam de mutex nenhum** — só pode haver UM emissor por vez no
processo inteiro. É a diferença mais instrutiva entre este modelo e `AlertDatalink`: o mesmo
mecanismo de evento, dois contextos de concorrência bem diferentes.

## Por que o payload mora em `models/events/`, não aqui

Ver o cabeçalho de `../../events/payloads/EID_PING/PingMessage.hpp` e
`../../events/README.md`, seção "Por que o payload mora aqui, numa `shared_library()`": um
`dynamic_cast` de um `base::Object*` recebido de OUTRO `.so` só é seguro se a classe do payload
vier de uma biblioteca linkada por **ambos** os lados — do contrário cada plugin compilado com
`gnu_symbol_visibility: hidden` enxerga seu próprio `type_info` para o "mesmo" tipo, e o
`dynamic_cast` falha silenciosamente. Como `Beacon` é o único consumidor de `PingMessage` hoje
(emissor e receptor são a mesma classe, no mesmo `.so`), esse risco especificamente não se
materializaria aqui — mas a convenção do repositório é a mesma para todo evento, não uma decisão
caso a caso: um evento que algum dia precisar ser tratado por um SEGUNDO plugin (o próximo passo
óbvio de exercício: um segundo modelo que só trata `EID_PING`, sem emitir) já encontra o payload no
lugar certo, sem precisar mover nada.

## Por que `PingMessage` também entra no `factoryNames()`/`provides:` deste plugin

Não é necessário para `Beacon::broadcastPing()` funcionar — o payload nasce programaticamente
(`new events::PingMessage()`), nunca por nome via `.edl`. É o mesmo motivo de
`models/players/air/A-4` acrescentar `TacticalAlert` ao seu: `xplugin::pluginMetaObjects()` (a fonte da
aba Memória do `./app`) só enxerga o que cada plugin **declara**, e um payload usado de verdade por
um modelo merece aparecer ali como qualquer outra classe dele.

## O que fica de fora, deliberadamente

- **Alcance/lado não são filtrados** — mesmo caminho (b) de `AlertDatalink::broadcastAlert()`: o
  broadcast direto alcança todo player local ativo, sem noção de distância nem de time. Filtrar
  por alcance seria replicar `RfSensor`/`Gimbal`, que já existem para isso — não é o que este
  modelo quer exercitar.
- **Nenhuma integração com `xboard`** — `libs/xboard::Readout` existe para publicar o que um
  agente UBF **decidiu** (`bt=`/`dec=`); `Beacon` não decide nada, só troca mensagens. Forçar uma
  chamada a `xboard::setBehaviorLabel()` aqui seria cosmético, não uma obrigação real (ver
  `models/template/docs/CONTRATO.md`, seção 3 — a obrigação é sobre modelos que decidem via UBF).
