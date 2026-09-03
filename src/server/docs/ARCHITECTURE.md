# `src/server` — notas de arquitetura

Complementa [../README.md](../README.md) (o contrato HTTP, o exemplo de corpo, a fronteira de
confiança). Este documento junta o **porquê** das decisões estruturais e as armadilhas
encontradas construindo isto — para quem for mexer aqui depois sem reconstruir o raciocínio do
zero.

## Por que dois executáveis, e por que nenhuma segunda `Station` no mesmo processo

`server` nunca linka `mixr_dep`. Cada `POST /simulate` dispara `sim-runner` como processo novo
(`fork`+`execv`, nunca `system()`/shell), que constrói **uma** `Station`, roda os frames pedidos
e sai. Essa separação não é só isolamento de falha — é o único caminho já comprovado neste
repositório: `shared/xplugin` nunca teve hot-reload de plugin testado dentro de um processo
vivo, e o próprio `./app` evita reconstruir uma segunda `Station` no mesmo processo, preferindo
`execv()` de si mesmo (`app/Respawn.hpp`, ver `CLAUDE.md`). Construir Stations sucessivas dentro
do `server` seria terreno nunca exercitado.

Consequência que cai de graça: como `sim-runner` nunca declara `dataRecorder:`/Tacview (bloqueado
por `ScenarioUpload`) nem escreve em arquivo compartilhado (cada requisição tem seu próprio
`data/runs/<pid>-<id>/`), requisições concorrentes não colidem em porta de rede nem em arquivo —
`server` aceita conexões em paralelo com segurança.

## Por que o corpo do cliente é só `players: {}`, não um `.epp` inteiro

O agente de decisão (`FlightAgentTC`) mora **dentro** de cada player como último componente
(padrão de `src/poc/multi-thread`), não na `Station` como `SimAgent` (padrão de
`src/poc/single-thread`) — de propósito: isso deixa cada player autocontido num bloco só, sem uma
segunda seção em outro lugar do arquivo para casar por nome (`actorPlayerName:`). O cliente nunca
precisa saber que existe um `PluginLoader`, um `WorldModel`, terreno ou `numTcThreads` — só
descreve quem voa e como decide. `configs/scenario_prefix.epp.in` + `scenario_suffix.epp.in`
resolvem o resto, com o mesmo mecanismo de substituição textual `@TOKEN@` que
`app/src/app/ScenarioTemplate.cpp` já usa para `@NUM_TC_THREADS@` (`ScenarioAssembler.cpp`, uma
implementação própria porque aqui são três arquivos concatenados, não um `.epp.in` só).

## Fronteira de confiança — por que a lista de bloqueio existe

Documentado em detalhe no `README.md`; o resumo é: `ScenarioUpload::validateScenarioBody()` é uma
checagem de substring (`PluginLoader`/`PluginModule`/`networks:`/`dataRecorder:`), não um parser
EDL. Ela impede o caso óbvio de abuso — apontar `PluginLoader` para um `.so` arbitrário é
`dlopen()` de código nativo arbitrário — mas não é uma sandbox. `edl_parser`, dentro de
`sim-runner`, é quem tem a palavra final sobre se o cenário é válido; um corpo que passa pela
heurística e ainda assim não compila vira `422` com o `stderr` do parser.

## Armadilhas confirmadas construindo isto — não redescobrir

1. **`nlohmann::json arr{nlohmann::json::array()};` NÃO copia o array — embrulha ele dentro de
   outro.** `nlohmann::json` tem um construtor de `initializer_list` que, recebendo um ÚNICO
   elemento que já é um `json`, o interpreta como o primeiro (e único) item de uma NOVA lista —
   `arr` vira `[[]]`, não `[]`. Medido rodando: a resposta de `/simulate` saía com um `[]` extra
   como primeiro elemento de `players`, antes dos objetos de verdade
   (`TelemetryJson.cpp::fleetToJson`). O mesmo padrão apareceu em `HttpServer.cpp` com
   `nlohmann::json body{errorBody(...)}` — ali o sintoma é pior, porque `body["exitCode"] = ...`
   logo depois lança `type_error` (chave string num array). **Correção: sempre `=`
   (`nlohmann::json x = expressao;`), nunca `{}` quando o valor já é um `nlohmann::json`.**
2. **`httplib::Server::set_error_handler` roda para TODO status >= 400 — mesmo quando a própria
   rota já escreveu um corpo de erro de propósito.** Sem checar `res.body.empty()` antes de
   sobrescrever, o handler global trocava os corpos de `400`/`422` (com `details`/`stderr` úteis)
   por um `{"error":"not_found"}` genérico, mantendo o status code certo — o que torna o bug fácil
   de não notar só olhando o `HTTP %{http_code}` do `curl`. Medido rodando: os quatro testes de
   validação da suíte `server-http` (abaixo) pegam isso na hora, porque conferem o CORPO, não só
   o status.
3. **O `.epp` montado não pode ir para um caminho fixo.** A primeira versão de `main_runner.cpp`
   escrevia sempre em `./src/server/data/runs/scenario.generated.epp` — inofensivo com uma
   requisição de cada vez, mas duas simulações concorrentes disputariam o mesmo arquivo.
   Corrigido: o `.epp` montado vai para o MESMO diretório do corpo recebido
   (`generatedPathFor(opts.bodyPath)`), que já é por-requisição (`server` cria um
   `data/runs/<pid>-<id>/` novo a cada `POST /simulate`).
4. **A stdout do `sim-runner` não é 100% controlada por ele.** `WorldModel::reset()` imprime
   `"Loading Terrain Data..."` incondicionalmente (nativo, sem flag para desligar — mesma
   armadilha já documentada no `CLAUDE.md` raiz para os `check-*` da suíte de determinismo).
   `sim-runner` nunca inicializa `xlog` e nenhum outro módulo seu imprime nada no caminho de
   sucesso, mas essa linha nativa ainda pode aparecer. Por isso `server` lê a **última linha
   não-vazia** da stdout capturada, não o corpo inteiro — mesma tolerância que os alvos
   `check-single-thread`/`check-multi-thread` já praticam com `grep '^frame='`.

## Testes

`tests/` (dentro desta pasta) tem duas camadas:

- **`unit/`** — gtest, unit-test-by-inclusion (linka o `.cpp` sob teste direto, sem MIXR, mesmo
  padrão da suíte `domain` do host): `test_scenario_upload.cpp` (a lista de bloqueio) e
  `test_scenario_assembler.cpp` (concatenação prefixo+corpo+sufixo e a substituição de
  `@NUM_TC_THREADS@`). Registrados em `tests/meson.build` (raiz) como
  `server-scenario-upload`/`server-scenario-assembler`, porque é lá que `gtest_dep` já está
  resolvido — `src/server` em si não linka gtest.
- **`http/run_server_test.py`** — contrato HTTP fim-a-fim: sobe o `server` de verdade (que por
  sua vez dispara o `sim-runner` de verdade) numa porta livre, roda a fixture
  `http/fixtures/valid_players.epp` e os cinco casos de erro do `README.md`. Registrado como
  `server-http`, `is_parallel: false` (como toda suíte que roda um binário da poc — JSBSim
  incluso).

Rodar só esta suíte: `meson configure build -Dtests=true && meson test -C build --suite server`
(precisa de `make install` antes, para `libflight_tc.so`/`flight_tree.xml` estarem em `dist/` —
`make test`, que já encadeia `install`, cobre isso sozinho).
