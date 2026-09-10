# Arquitetura do modelo C-130

## O que este modelo faz

Um C-130 que **navega de verdade** por `Route`/`Steerpoint` nativo do MIXR e é **capaz de liberar
um paraquedista** a partir de um steerpoint. Nenhum combate, nenhum sensor próprio, nenhum
datalink — em comparação com `models/players/A-4` (o modelo de produção completo, com árvore de
quatro ramos, evasão, RL, ONNX, Python embarcado), este modelo é deliberadamente enxuto: uma árvore
de **um nó só**.

## As camadas

```
domain/    -- DTOs puros. Sem MIXR, sem BehaviorTree.CPP.
              FlightCommand -- rumo/altitude/velocidade comandados
              WorldView     -- a leitura do frame (posicao, guiagem nativa) -- SEM contato/
                               alerta/arma, ao contrario do WorldView de A-4: este modelo nao
                               tem nada disso
              geometry      -- wrap180/wrap360, usados pelo limitador de guinada

bt/        -- a ARVORE: um no so, Navigate (sem Fallback). Le
              WorldView::hasNavSteering/navTrueBrgDeg/navCmdAltM/navCmdSpeedKts (ja calculados
              todo frame por Route::autoSequencer()/Steerpoint::compute(), independente do
              agente) e produz um FlightCommand, com um limitador de taxa de guinada (a mesma
              tecnica documentada em A-4: comandar a marcacao instantanea direto, sem limitar,
              diverge perto do alvo -- perseguicao pura sem termo de avanco).

ubf/       -- os tres papeis do UBF:
              FlightState  -- percepcao: le o AirVehicle, monta o WorldView
              BtBehavior   -- decisao: constroi a arvore (preguicoso, uma vez) e tica
              FlightAction -- atuacao: comanda o Autopilot nativo (headingHoldMode/
                              altitudeHoldMode/velocityHoldMode + setCommandedHeadingD/
                              AltitudeFt/VelocityKts) e escreve no xboard (obrigatorio -- ver
                              abaixo)

xnative/   -- a cola de registro + as duas classes novas:
              FlightAgentTC             -- o agente de tempo critico (fase 3 do frame)
              ActionParatrooperRelease  -- a liberacao generica (ver secao propria)
              ParatrooperPlaceholder    -- entidade nativa provisoria (ver secao propria)
              factory.{hpp,cpp}         -- registro das 6 classes, nomes "C130*"
```

Mesma divisão de testabilidade de `A-4`: `domain/` não linka MIXR nem em teste; `bt/` linka
BT.CPP + `domain/`, não MIXR (por isso `tests/tree/` carrega a árvore de PRODUÇÃO contra um
`FakeDecisionContext`, sem `Station`); `ubf/`+`xnative/` linkam MIXR, mas nunca levantam uma
`Station` de verdade (`tests/native/` usa um `Bench` — `WorldModel`+`AirVehicle` construídos
direto com `new`).

## Por que namespace `mixr::models::xC_130`, e não `domain::` solto

`A-4` usa um `domain::` global (chegou primeiro, é a exceção histórica documentada em
`../template/docs/CONTRATO.md` §6 — "não o padrão a copiar"). Este modelo — como qualquer cópia do
`template/` — aninha tudo sob `mixr::models::xC_130::`: um cenário pode carregar mais de um plugin
no mesmo processo, e dois tipos com o mesmo nome qualificado (`domain::Foo`) em dois `.so`s
`RTLD_LOCAL` distintos têm o mesmo símbolo *mangled* — a comparação de `type_info` deste toolchain
degrada para `strcmp`, e colidiria silenciosamente.

## Nomes de fábrica com prefixo `C130` — não o mesmo problema, uma segunda guarda

O namespace C++ evita colisão de RTTI; nomes de fábrica são STRINGS separadas, comparadas por
`tests/guard/check_colisao_fabrica.py` **par a par entre todos os modelos sob `models/`**,
independente de alguma vez serem carregados juntos no mesmo `.edl`. Reaproveitar os nomes de A-4
(`"FlightState"`, `"BtBehavior"`, `"FlightAction"`, `"FlightAgentTC"`) — mesmo sob namespace C++
diferente — derrubaria essa guarda na hora (a mesma classe de incidente já documentada para
`ThreadTagProbe`, A-4×o extinto modelo `missile`). Por isso as seis classes deste modelo se
registram como `"C130FlightAgentTC"`, `"C130ActionParatrooperRelease"`,
`"C130ParatrooperPlaceholder"`, `"C130FlightState"`, `"C130BtBehavior"`, `"C130FlightAction"` — e
todo `.edl` que carrega este modelo precisa nomear as classes assim, não pelos nomes "curtos".

## O piloto automático JSBSim próprio — por que ele precisou ser escrito

`shared/data/jsbsim/aircraft/C130/C130.xml` (o dado vendorizado, upstream JSBSim) não tem bloco
`<autopilot>` nenhum. Investigado antes de escrever qualquer EDL: `mixr::models::JSBSimModel`
(`contexts/src/mixr/src/models/dynamics/JSBSimModel.cpp`) só liga `hasHeadingHold`/
`hasAltitudeHold`/`hasVelocityHold` (uma vez, em `reset()`) se os nós `ap/heading_hold`+
`ap/heading_setpoint` (e os pares de altitude/velocidade) **já existirem** na property-tree do
JSBSim — o que só acontece se a aeronave declarar um `<autopilot>` próprio. Sem essas flags,
`JSBSimModel::dynamics()` pula, todo frame, o bloco inteiro que empurraria o comando do `Autopilot`
nativo do MIXR para o JSBSim — silenciosamente. Pior: `Autopilot::headingController()`/
`altitudeController()` escolhem o ramo só pelo PRÓPRIO estado (`isHeadingHoldOn()`/
`isNavModeOn()`), então ligar `headingHoldMode:true` sem isso **trava o manche** em vez de deixá-lo
livre para o piloto manual — o mesmo mecanismo do qual `navMode:true` (seguir Route/Steerpoint
nativamente, sem agente) também depende.

`data/jsbsim/aircraft/C130/`:
- `C130.xml` — **cópia curada** do vendorizado (não o original em `shared/`), com duas edições:
  `<autopilot file="c130ap"/>` inserido antes de `<flight_control>`, e três `<input>ap/elevator_cmd
  </input>`/`<input>ap/aileron_cmd</input>`/`<input>ap/rudder_cmd</input>` acrescentados aos
  summers de arfagem/rolagem/leme — `JSBSimModel::dynamics()` nunca faz essa ponte em C++, é
  sempre responsabilidade do XML da própria aeronave (confirmado comparando contra
  `models/players/A-4/data/jsbsim/aircraft/A4/A4.xml`, que tem a mesma edição pelo mesmo motivo).
- `c130ap.xml` (novo) — adaptado de `models/players/A-4/data/jsbsim/aircraft/{c310,A4}/*ap.xml`:
  malha de rumo/altitude do `c310ap.xml`, canal de autothrottle (4 motores) e SAS (amortecedores +
  nivelador de asas) do `a4ap.xml`.

**Rumo, altitude e velocidade, medido rodando `src/poc/c130-airdrop/` e `sandbox/C-130-6DOF/` por
600 s simulados sem crash, funcionam.** Nem sempre foi assim — histórico, corrigido, não
redescobrir: a primeira versão de `data/jsbsim/engine/t56.xml` (copiado sem alteração do
vendorizado `shared/data/jsbsim/engine/`) estava pareada com `t56_prop.xml`, um `<propeller>`
(espera potência/HP) contra um `t56.xml` `<turbine_engine>` (calcula empuxo direto) — um
descompasso de tipo dentro do próprio dado JSBSim vendorizado (Aero-Matic), não algo introduzido
por este modelo. O combustível era consumido normalmente com o throttle (prova de que o comando
chegava ao motor), mas o empuxo efetivo que chegava na célula ficava perto de zero — rodando tempo
suficiente (~160 s), a aeronave estolava e colidia com o terreno. **Corrigido** trocando o
thruster para `direct` (mesmo padrão já comprovado em `models/players/A-4/data/jsbsim/aircraft/A4/
A4.xml`, cujo `J52` também é `<turbine_engine>`) — ver o achado completo no cabeçalho de
`c130ap.xml`. Os ganhos PID/lei de subida/limites do `Autopilot` continuam chute inicial (herdados
de `c310ap.xml`/`a4ap.xml` por semelhança de regime) e têm folga/atraso perceptível ao seguir
mudanças de altitude — funcional e seguro com margem de terreno generosa, não uma calibração
fina.

## A liberação de paraquedista — por que é tratada como ARMA

Investigado antes de desenhar: o mecanismo nativo de "colocar um novo `Player` vivo na simulação"
(`AbstractWeapon::release()` → `clone()` → `WorldModel::addNewPlayer()`) é genérico, mas o único
CAMINHO já wireado a um `Steerpoint::action:` é via `StoresMgr` (`ActionWeaponRelease`/
`ActionDecoyRelease`, cada uma hardcoded a uma família de armamento nativa). Não existe uma
`ActionPlayerRelease` genérica. E, mais importante: um paraquedista **não decide nada** — ele cai,
o paraquedas abre, ele pousa. Isso não precisa de `AbstractState`/`AbstractBehavior`/
`AbstractAction` (UBF) nenhum — precisa só de dinâmica (queda + arrasto), exatamente o que
`mixr::models::Effect` (a base de `Chaff`/`Decoy`/`Flare`) já oferece.

`xnative::ActionParatrooperRelease` (`Action` própria deste modelo) reproduz o caminho nativo de
`ActionDecoyRelease::trigger()` (`mgr->findContainerByType(Player)` → `getStoresManagement()` →
`StoresMgr::getWeapons()`), mas **genérico**: em vez de chamar `releaseOneDecoy()` (hardcoded a
`Decoy`), percorre a lista de armas e libera a primeira `AbstractWeapon*` disponível
(`isInactive() || isReleaseHold()`) cujo `Player::getType()` bata com o slot `storeType:` (default
`"PARATROOPER"`) — usando `Stores::releaseWeapon(AbstractWeapon*)`, o método público e genérico que
`releaseOneBomb`/`releaseOneDecoy` já chamam por baixo. **Nunca há `dynamic_cast` para uma classe
concreta de paraquedista** — é isso que deixa a troca futura pelo `models/players/paratrooper` real
livre de mudança de C++ aqui: só o EDL muda (a classe declarada na estação de `stores:` + o
`provides:` do cenário).

`xnative::ParatrooperPlaceholder` (`Effect` trivial, `EMPTY_SLOTTABLE`) é o que ocupa essa estação
até o modelo de verdade existir. Nome de fábrica `"C130ParatrooperPlaceholder"` — **não**
`"Paratrooper"` — de propósito: registrar esse nome colidiria com `check_colisao_fabrica.py` assim
que o modelo real nascer com esse nome mais óbvio.

**`( OnboardComputer )` é obrigatório no player**, mesmo sem nenhum sensor — confirmado lendo
`mixr::models::Route::triggerAction()`: sem um `OnboardComputer`, a chamada `obc->triggerAction()`
nunca acontece, e a `Action` do steerpoint nunca dispara, em silêncio.

## A obrigação que falha em silêncio: escrever no `xboard`

`ubf::FlightAction::execute()` termina com:

```cpp
xboard::setBehaviorLabel(player->getID(), label);
xboard::bumpDecisionCount(player->getID());
xboard::setThreadTag(player->getID(), xboard::threadTag());
```

Não é exigido pelo compilador nem pelo carregador de plugin — sem essas chamadas, o host sobe, o
cenário parseia, tudo passa, e a tela de status/o dump `-deterministic` mostram `bt=--`/`dec=0`
para sempre, sem erro em lugar nenhum. Ver `../template/docs/CONTRATO.md` §3.

## O que este modelo não tem, de propósito

- **Combate/evasão/alerta tático** — sem `AlertDatalink`, sem contato de radar, sem `ThreatPolicy`.
  Este modelo só navega.
- **Sensores próprios** — `( OnboardComputer )` fica sem nenhum track manager; existe só para a
  `Action` do steerpoint disparar.
- **RL, ONNX, Python embarcado, terreno como piso ativo** — nada disso está aqui. `A-4` é a
  referência completa para essas camadas, se algum dia fizerem sentido para o C-130.

## Ler também

- [`../README.md`](../README.md) — como compilar, testar e instalar este diretório sozinho
- [`../../../src/poc/c130-airdrop/README.md`](../../../src/poc/c130-airdrop/README.md) — o cenário
  de demonstração
- [`../template/docs/CONTRATO.md`](../template/docs/CONTRATO.md) — a lista completa e autoritativa
  do que um modelo precisa fazer
- `../../../CLAUDE.md`, seção "O MODELO é um plugin, construído numa etapa PRÉVIA"
