# `libs/xlog` — log com nível, sintaxe de stream e buffer em memória

`LOG(NIVEL) << ...;` — persiste em arquivo e console, e alimenta a aba "Log" do `./app` sem
precisar de `Station` nem de `.edl`. Não é o `mixr::recorder` — ver o porquê abaixo.

## Como se usar

Pura API C++, sem factory e sem slot: nenhum cenário declara `libs/xlog`. Um `main.cpp` chama
`init()` uma vez, cedo, e daí em diante qualquer arquivo do processo — host **ou** plugin
carregado por `dlopen` — só inclui o header e usa a macro:

```cpp
#include "xlog/Log.hpp"

LOG(WARNING) << "algo aconteceu: " << valor;   // DEBUG / INFO / WARNING / ERROR
```

É exatamente assim que `models/player/A4/src/ubf/FlightAction.cpp` reporta a transição de
comportamento de um avião:

```cpp
LOG(INFO) << "[FlightAction] " << player->getName()->getString()
          << ": " << before.label << " -> " << label
          << "  (hdg=" << command.headingDeg
          << "deg alt=" << command.altitudeM
          << "m vel=" << command.speedKts << "kt)";
```

O `main.cpp` do `./app` é quem abre o arquivo, uma vez por processo, cedo — e é quem desativa
tudo no modo determinístico:

```cpp
mixr::xlog::init((scenarioDir / "data" / "logs" / (cenario.key + "_" + runId + ".log")).string());

if (opts.isDeterministic()) mixr::xlog::setLoggingEnabled(false);
```

`setLoggingEnabled(false)` desliga **as três coisas** — console, arquivo e o buffer em memória —
porque uma linha de log carrega timestamp de parede, fora do que um dump `frame=` comparável
tolera. `setConsoleEnabled(false)` é mais fino: desliga só a cópia em `std::cout`, mantendo
arquivo e buffer vivos. Existe para quem é dono do terminal — `DashboardLoop.cpp` chama isso no
início de `runDashboard()` (e `setConsoleEnabled(true)` de volta ao sair) porque o FTXUI assume um
painel em tela cheia, e uma linha escrita direto em `stdout` no meio disso suja o desenho: o FTXUI
não sabe que alguém escreveu por baixo dele e não redesenha aquela região.

## Por que não é o `mixr::recorder` de verdade

Investigado antes de escrever qualquer linha: o schema `DataRecord.proto` é fechado — nenhuma
mensagem por-evento tem campo de texto livre, nem o `MarkerMsg` (só dois `uint32`) — e o único
ponto de entrada público do gravador, `AbstractDataRecorder::recordData(id, pObjects[4],
values[4])`, não tem overload de string. Remendar o `.proto` vendorizado é o tipo de invasão que
este projeto evita (MIXR é dependência binária, não objeto de desenvolvimento).

O que **é** reaproveitado: `mixr::recorder::PrintHandler` (a base de `TabPrinter` e companhia),
mas usado **por fora** do pipeline `recordData()`/REID/protobuf. `Log.cpp` instancia um
`PrintHandler` direto em C++ (`new recorder::PrintHandler()` + `setFilename()`) e chama
`printToOutput(const char*)` — que escreve num `std::ofstream` próprio, sem nunca passar por
`processRecordImp()`/`DataRecordHandle`, então nunca esbarra no schema fechado. Já é dependência
transitiva de `mixr_dep` (`mixr-recorder` no `Requires:` do `mixr.pc`, a mesma lib que
`libs/xtacview` linka) — nenhuma dependência nova.

## Por que é `shared_library()`, não estática

Ao contrário da maioria de `libs/x*`, `xlog` cruza a fronteira de plugin: o modelo
(`models/player/A4`, `.so` aberto por `dlopen`) chama `LOG(...)` ao carregar a árvore de
comportamento (`ubf/BtBehavior.cpp`) e a cada decisão atuada (`ubf/FlightAction.cpp`). Com duas
cópias da lib, o `setLoggingEnabled(false)` que `main.cpp` chama sob `-deterministic` não
alcançaria o lado do plugin, e o modo comparável passaria a emitir linhas com timestamp de parede
— quebrando exatamente o que o modo existe para garantir.

Efeito colateral, e é o motivo estrutural do buffer em memória (abaixo): como há **uma** cópia só
no processo, o `LOG(...)` emitido de dentro do `.so` do modelo cai no **mesmo** buffer que o do
host. A aba "Log" do `./app` mostra os dois sem nenhuma ponte extra — confirmado removendo
`flight_tree.xml` do lugar: as 4 linhas `LOG(ERROR)` de `BtBehavior` (uma por falcon, de dentro de
`libflight_tc.so`) aparecem na aba sem código nenhum do lado do host.

## O buffer em memória

Além de console e arquivo, toda linha entra num buffer circular (`std::deque<Entry>`) das últimas
`kMemoryCapacity` (500) entradas — é a fonte da aba "Log". `Entry` guarda os campos **separados**
(`seq`/`level`/`time`/`text`), não a linha já formatada: quem exibe quer colorir por nível e
alinhar o carimbo em coluna própria.

- **Por que aqui, e não um `tail` do arquivo**: o arquivo é escrito por um `PrintHandler` com
  flush próprio (reler o que acabou de ser escrito seria corrida contra o `ofstream`); e esta lib
  já é o ponto por onde toda linha passa, com o mutex que já serializa os escritores.
- `lastSeq()` devolve o `seq` da linha mais recente (barato) — quem exibe só recopia o buffer
  (`snapshot()`) quando esse número muda, em vez de a cada redesenho.
- Passada a capacidade, a mais antiga sai (`pop_front`); `seq` nunca reinicia, então
  `entries.front().seq` já diz quantas linhas escorreram.

## Armadilhas confirmadas

1. **Um mutex único serializa tudo** (`g_mutex`, em `Log.cpp`) — necessário porque mais de uma
   aeronave decide em paralelo, uma por thread do pool de tempo crítico (`FlightAgentTC`), e todas
   podem logar no mesmo frame. Sem lock as linhas se entrelaçariam no `std::ofstream` (que não é
   thread-safe sozinho).
2. **`g_sink->isOpen()` não é `g_sink != nullptr`.** Quando o arquivo falha ao abrir (diretório
   `data/logs/` ausente — o mesmo jeito mudo que `TacviewOutput` falha sem `data/recordings/`),
   `PrintHandler::printToOutput()` cai no próprio fallback nativo e escreve em `std::cout` **por
   fora** de `g_consoleEnabled`. Sem checar `isOpen()`, duas coisas quebravam ao mesmo tempo: com
   console ligado, a linha saía **duas vezes**; com console desligado (o caso do `./app`, que
   desliga exatamente para o FTXUI não ter o desenho sujado por baixo), a linha vazava do mesmo
   jeito — o oposto do que `setConsoleEnabled(false)` promete.
3. **`init()` apaga o arquivo anterior de propósito** (`std::remove()` antes de abrir).
   `PrintHandler::openFile()` nunca reescreve um arquivo que já existe ("we don't want to over
   write good data") — ele tenta `_v01`, `_v02`, ... até 99, o que faz sentido para uma GRAVAÇÃO
   de dados, mas não para um log de diagnóstico por PROCESSO: cada respawn do `./app` (carregar
   cenário/reiniciar, ambos um `execv()` de si mesmo) chamaria `init()` de novo, empurrando a
   versão pra frente até as 99 esgotarem — e todo log novo parar de ir pro disco, em silêncio.
4. **`data/logs/` precisa existir no disco antes de `init()`.** `PrintHandler::openFile()` não
   cria diretório, só abre o `ofstream` — sem o diretório, falha muda (mesma classe de bug do
   `TacviewOutput`/`data/recordings/`).
5. **`Level` é `enum class`, não `#define`s soltos** — evita colisão com macros de sistema
   (`ERROR`/`DEBUG` são armadilhas clássicas em outros contextos, como `wingdi.h` no Windows).
   Irrelevante neste projeto (Linux-only, sem `-DDEBUG`), mas o desenho já nasce sem a pegadinha.

## Testes

`app/tests/test_log_panel.cpp` (alvo `app-log`, suíte `domain`): ordem do buffer (mais antigo →
mais novo), `seq` monotônico, descarte do mais antigo passada a capacidade, o desligamento não
registrando nada, e as duas variantes do gotcha de `isOpen()` acima — console ligado não duplica
quando o arquivo falha ao abrir, console desligado não vaza quando o arquivo falha ao abrir.
