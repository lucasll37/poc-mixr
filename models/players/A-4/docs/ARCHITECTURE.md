# `flight` — notas de arquitetura

Complementa [../README.md](../README.md) (visão geral, as quatro camadas, o `#ifdef` que separa
`libflight.so`/`libflight_tc.so`). Este documento junta as decisões de calibração e as armadilhas
específicas deste modelo que, de outra forma, só existiam espalhadas em
[../../../CLAUDE.md](../../../../CLAUDE.md) — útil para quem abriu só `models/players/A-4/` (ver o
`Makefile` ao lado para o build autocontido) e não tem o resto do repositório em mente.

## A aeronave é dado do MODELO, não do cenário

`data/jsbsim/` (o A-4, desde a troca do c310 — ver `data/jsbsim/aircraft/A4/a4ap.xml` para o
autopilot escrito para esta PoC-mixr) e `configs/flight_tree.xml` são publicados junto com o `.so`
(`install_subdir()`/`install_data()` no `meson.build`, para `<prefix>/share/mixr-plugins/flight/`)
— não moram em `src/poc/<poc>/data/` de nenhuma poc. Não é só arrumação: `domain/`/`bt/` deste modelo
são calibrados **para a aeronave que voa aqui** especificamente:

- `maxClimbRateMps`/`maxRateOfTurnDps` do `Autopilot` (ver `include/domain/`) — DECORATIVOS para
  `JSBSimModel` (que ignora o 2º/3º parâmetro de `setCommandedHeadingD`/`setCommandedAltitude`/
  `setCommandedVelocityKts`, confirmado lendo `JSBSimModel.cpp`), mas mantidos coerentes com a
  aeronave por documentação.
- A folga de `domain/TerrainFloor.hpp` contra o piso anti-CFIT é uma margem de segurança sobre o
  terreno, independente do tipo de aeronave — não precisou mudar na troca para o A-4.
- Os limiares de combustível em `bt/nodes/FuelLowCondition.cpp`.

Trocar de aeronave sem recalibrar pelo menos as velocidades comandadas (`patrolSpeed`/`rtbSpeed`/
`evadeSpeed`/`supportSpeed`, no `.edl` de cada cenário) e o próprio `<autopilot>` JSBSim (que
`JSBSimModel` exige para os modos de hold funcionarem — ver `JSBSimModel.cpp`) não faria sentido —
por isso a aeronave viaja **com** o modelo, não com o cenário.

**Limite conhecido, medido rodando na troca c310 -> A-4 (não redescobrir):** os dados aerodinâmicos
do A-4 em `shared/data/jsbsim/aircraft/A4/A4.xml` (Aeromatic, `shared/data/jsbsim/README`-adjacent)
têm um modo látero-direcional levemente instável em espiral (`Clb·Cnr < Clr·Cnb` com os
coeficientes do próprio arquivo — `Clb=-0.1 Cnr=-0.15 Clr=0.15 Cnb=0.12`), e o `JSBSimModel::
reset()` nativo não roda `FGTrim` (só `RunIC()` — confirmado no fonte), então a aeronave nasce
destrimada. O c310 tolerava as duas coisas por ser dócil e bem amortecido; o A-4 (mais rápido, mais
leve, superfícies mais potentes) não. `a4ap.xml` ganhou um SAS sempre-ativo (nivelador de asas +
amortecedores de taxa em rolagem/arfagem, independentes de `ap/heading_hold`/`ap/altitude_hold`) e
um trim estático de profundor — sem eles a aeronave diverge em rolagem/arfagem em poucos segundos,
mesmo com o piloto automático desligado. Com o SAS, o determinismo (`check-single-thread`/
`check-multi-thread`, 2000 frames, 1/2/4 threads) passa byte-a-byte, mas o VOO ainda deriva
lentamente (dezenas de segundos) para fora do nível antes de o nivelador reafirmar o controle — não
é uma regressão de determinismo, é uma característica aerodinâmica real do dado Aeromatic,
documentada aqui em vez de escondida. Recalibrar o SAS (ou re-exportar a aeronave com um ajuste de
Clr/Cnb) fica registrado como trabalho futuro, não feito.

## As quatro camadas — direção de dependência

Ver o diagrama em [../README.md](../README.md). O ponto que vale repetir: `bt/` não inclui nada de
`ubf/`, nada do MIXR e nada de `xnative/`, e o namespace (`bt_nodes`) fica deliberadamente fora de
`mixr::`. É o que permite `tests/tree/` carregar o `configs/flight_tree.xml` **de produção** contra
um `FakeDecisionContext`, em ~10 ms, sem linkar uma lib do MIXR — a suite mais barata e mais
repetida durante o desenvolvimento da árvore.

## O log do modelo (`LOG(...)` em `ubf/FlightAction.cpp`)

`libs/xlog` é uma `shared_library()`, então há **uma cópia no processo** e o `LOG(...)` emitido
de dentro deste `.so` (aberto por `dlopen`) cai no mesmo buffer/arquivo do host — é o que faz a aba
"Log" (F5) do `./app` mostrar o que o modelo registra, sem nenhuma ponte. Nas outras pocs
(`single-thread`/`multi-thread`), que não têm aba, as mesmas linhas saem no console e no
`data/logs/*.log`. Sob `-deterministic` o `main.cpp` chama `setLoggingEnabled(false)` e nada é
emitido — os dumps comparáveis não mudam.

`FlightAction::execute()` é o ponto certo para isso: é a única atuação comum aos dois agentes
(`SimAgent` de background e `FlightAgentTC` do pool T/C) e já é onde o rótulo vencedor chega. O
que se registra: transição de comportamento (`PATROL -> EVADE`), alerta tático, lançamento de
míssil (e as duas formas de ele não acontecer) e um batimento a cada `kHeartbeatEveryDecisions`
decisões atuadas.

**A armadilha, medida e corrigida:** `execute()` roda **até 50 Hz por aeronave**, e nem todo campo
da ação é um evento — `broadcast` (o pedido de alerta tático) fica **ligado** enquanto a aeronave
evade. Logar direto no `if (broadcast)` deu ~50 linhas/s por aeronave: em 20 s de intercepto o
buffer de 500 entradas já tinha girado três vezes e engolido justamente as transições. Por isso
todo log daqui passa por uma regra de **borda** (`changedFor()`, um mapa estático por player, com
mutex porque os agentes decidem em paralelo) ou de **contagem** (o batimento), nunca por decisão.

## Editando a árvore com o Groot — o gerador `dump-tree-model`

O Groot (editor visual das árvores do BehaviorTree.CPP — instalação em
[`../INSTALL.md`](../INSTALL.md) §4) só reconhece um nó customizado (`FuelLow`, `ContactDetected`,
`OnnxPolicy`, `PyDecide`, ...) se o `.xml` tiver um bloco `<TreeNodesModel>` descrevendo ID e
portas de cada um — sem ele, carregar qualquer `flight_tree*.xml` recusa com *"This model has not
been registered"*. Esse bloco não é mantido à mão: `tools/dump_tree_model.cpp` (alvo Meson
`dump-tree-model`, registrado em [`../tests/meson.build`](../tests/meson.build)) monta a MESMA
`BT::BehaviorTreeFactory` que o modelo de produção monta — `bt_nodes::registerNodes()` +
`bt_nodes::registerSdkNodes()`, os mesmos dois que `xnative/factory.cpp` chama — e devolve
`BT::writeTreeNodesModelXML(factory)`, a função **nativa** do BT.CPP que lê o manifesto (ID +
portas) de cada nó registrado. `behavior=nullptr` num `NodeContext{}` vazio é seguro aqui:
`registerBuilder<T>()` só guarda um construtor (lambda) na factory — nenhum nó chega a ser
instanciado, só o manifesto é lido.

### Três formas de usar

```bash
# compilar (ou recompilar, depois de registrar um no novo em bt_factory.cpp/bt_factory_sdk.cpp)
meson compile -C build dump-tree-model

# 1. so o fragmento <TreeNodesModel>...</TreeNodesModel> -- para colar dentro do <root> de uma
#    arvore ja existente. E' o modo que sync_tree_models.py usa por baixo.
./build/tests/dump-tree-model

# 2. um .xml COMPLETO, pronto pra abrir no Groot: uma arvore vazia (<Fallback name="root"/>,
#    builtin do BT.CPP -- nao precisa de modelo nenhum) + o mesmo <TreeNodesModel>, os dois no
#    mesmo <root>. Resposta a "como eu crio uma arvore nova com os nos que ja implementei".
./build/tests/dump-tree-model --skeleton MinhaArvore > /tmp/nova.xml

# 3. resincronizar os 5 flight_tree*.xml DE PRODUCAO de uma vez, depois de um no novo -- roda o
#    binario, extrai o fragmento, substitui o <TreeNodesModel> nos 5 arquivos.
python3 tools/sync_tree_models.py            # escreve
python3 tools/sync_tree_models.py --check    # so verifica -- exit 1 se algum estiver desatualizado
```

`--check` é o que o teste `tree-model-sync` (suíte `tree`, ver
[`../tests/meson.build`](../tests/meson.build)) roda a cada `make test`. Sem essa guarda, esquecer
de resincronizar depois de registrar um nó novo seria um "verde silencioso" — os 76 testes do
modelo continuam todos verdes, e o problema só aparece quando alguém tenta abrir a árvore no Groot
e recebe *"This model has not been registered"*. O teste passa o caminho de **verdade** do binário
(o objeto `dump_tree_model` do próprio Meson, não o default de `sync_tree_models.py`, que supõe um
diretório chamado `build`) — Meson já resolve isso e adiciona a dependência de build sozinho.

### Armadilhas confirmadas — não redescobrir

1. **Um nó novo em `bt_factory.cpp`/`bt_factory_sdk.cpp` não aparece nos XMLs sozinho.** Registrar
   a classe na factory C++ não toca em nenhum `.xml` — é preciso rodar `sync_tree_models.py` (ou
   deixar `make test` acusar via `tree-model-sync`) depois. Sem isso, o sintoma é indistinguível de
   "esqueci de registrar o nó" olhando só os XMLs.
2. **Nós que dependem do SDK (`OnnxPolicy`/`OnnxScore`/`PyDecide`, em `bt_sdk_sources`) exigem o
   SDK publicado** (`cd ../../.. && make configure && make sdk`) para o próprio
   `dump-tree-model` compilar — ele linka `sdk_dep` justamente para incluir esses três no
   manifesto; sem o SDK, o `meson setup` deste projeto falha antes de chegar a compilar nada.
3. **A saída de `writeTreeNodesModelXML()` já vem envolta num `<root>` próprio** (pensado para virar
   um `.xml` sozinho) — por isso `dump_tree_model.cpp` extrai só o miolo
   (`extractTreeNodesModel()`) antes de imprimir; colar a saída CRUA dentro de um `<root>` já
   existente duplicaria a tag e o Groot recusaria o arquivo.

## Armadilhas confirmadas — não redescobrir

1. **O motor NÃO precisa de `engine-autostart.xml`.** Isso valia para o c310 (pistão — `JSBSimModel`
   nativo nunca liga um motor a pistão sozinho, só `SetRunning(true)`/`InitRunning()`, insuficiente
   sem magneto/mistura/partida). O J52 do A-4 é turbina: `FGTurbine::InitRunning()` já força
   `Running=true` com N2 em idle, e o próprio giro relativo ao vento em voo faz o resto — confirmado
   lendo `FGTurbine.cpp` e rodando sem o systems file. Por isso `A4.xml` não o declara.
2. **`engRpmRaw` é polimórfico na unidade** entre motor a pistão (RPM absoluto), turbina (`%N2`) e
   turboprop (`%N1`) — o A-4 é `J52`, turbina, então `getEngRPM()` devolve `GetN2()` em
   **percentual** (~60-100), não RPM absoluto. Era pistão (`engIO470D`, `maxrpm 2625`) só enquanto o
   modelo era o c310.
3. **O piso anti-CFIT depende de `terrainElevReq: false`** (default) — com `true`,
   `Player::updateElevation()` pula a consulta ao banco SRTM e espera um gerador externo. Ver a
   seção "Terreno" do `CLAUDE.md` para a lista completa (10 armadilhas) sobre `SrtmHgtFile`.
4. **`wrap180()` tem borda em -180, não em +180** (`fmod(360,360)==0`) — `domain/ThreatPolicy`
   escolhe o lado da quebra por `relBearingDeg >= 0`, então um contato exatamente a ré quebra
   sempre para o mesmo lado. Comportamento travado por teste, não coincidência.

## Testes

`tests/{domain,tree,native}/` — ver [../README.md](../README.md) para a contagem e o que cada
camada prova. `make test` (neste diretório, via o `Makefile` autocontido) roda as três.
