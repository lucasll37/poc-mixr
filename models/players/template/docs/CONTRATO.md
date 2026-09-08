# O que um modelo precisa fazer

## Contexto, para quem está chegando agora

A aplicação deste repositório (o "host") não decide nada sozinha: ela carrega a lógica de
percepção/decisão/ação de uma biblioteca compilada à parte (um "modelo", também chamado de
"plugin"), aberta em tempo de execução via `dlopen`. Um cenário (arquivo `.edl`, o formato
declarativo do MIXR) diz qual arquivo `.so` carregar e quais classes esperar dele; o host nunca
viu o código-fonte desse `.so`.

Esse mecanismo de carga (o formato do descritor binário, a macro de registro, as flags de link)
está definido em [`libs/xplugin/PluginAbi.hpp`](../../../../libs/xplugin/PluginAbi.hpp) — mas
esse arquivo só cobre o **empacotamento**: como o `.so` se anuncia. Ele não diz o que a aplicação,
já rodando, de fato espera do modelo para funcionar de ponta a ponta. Esta página é essa lista —
tudo o que falta saber além do empacotamento, incluindo a única obrigação cuja falta **não gera
erro nenhum** (seção 3).

`mirror.cpp`, no diretório `../src/` (`../src/mirror.cpp`), é uma implementação completa e mínima
(~360 linhas) de tudo o que está aqui — o mirror de contrato que este repositório usa nos próprios
testes de plugin, herdado de `models/players/fixtures/stub` (removido). Leia esta lista primeiro;
use o `.cpp` como referência de como cada item fica em código. **Ele não é o scaffold copiável** —
esse é o resto deste diretório (`src/domain/`, `src/ubf/`, `src/xnative/factory.cpp`), que mostra a
separação em camadas em vez do atalho achatado que `mirror.cpp` usa.

## 1. Empacotar como plugin, não como biblioteca comum

- Compile como `shared_module()` no Meson, nunca como `library()` — um artefato linkável poderia
  acabar num `link_with:` de outra coisa, duplicando estado interno do MIXR no processo.
- Exporte o ponto de entrada **só** pela macro `MIXR_PLUGIN_DEFINE` (nunca escrevendo a assinatura
  `extern "C"` à mão): em um alvo com símbolos escondidos por padrão, uma assinatura manual vira
  invisível para quem carrega o `.so` em runtime, e o sintoma some longe do lugar do erro.
- Linke com `-Wl,--no-undefined` (o executável do host não exporta nada para um plugin chamar) e,
  se o seu modelo linkar alguma biblioteca **estática**, também `-Wl,--exclude-libs,ALL` (senão os
  símbolos dela vazam para fora do seu `.so`).

O `meson.build` deste diretório mostra as linhas mínimas que fazem isso (procure pelo bloco
`template_mirror_lib`, o exemplo mais próximo do mínimo — o `template_lib` de produção acrescenta
a separação em camadas por cima do mesmo esqueleto).

## 2. Fornecer exatamente as classes que o cenário pede

Quem decide **quais nomes de classe** e **quais parâmetros** (chamados de "slots" no MIXR) o seu
modelo precisa fornecer não é este documento — é o arquivo `.edl` do cenário que vai carregá-lo.
Esse arquivo tem um bloco assim:

```
( PluginModule  file: "libmeumodelo.so"
   provides: { NomeDaClasseA NomeDaClasseB ... } )
```

`provides:` precisa bater **exatamente** com o conjunto de nomes que o seu `.so` de fato registra
(nem a mais, nem a menos) — se não bater, o processo aborta na inicialização, com uma mensagem
dizendo o que o `.so` entregou. Não é uma falha silenciosa.

Cada nome também precisa derivar da classe-base do MIXR que o ponto de uso espera (ex.: um nome
usado no slot `state:` de um agente de decisão precisa herdar de `AbstractState`), e os slots que
o cenário passa para ele por EDL precisam existir, com o tipo certo.

**Exemplo real**, tirado do cenário de produção deste repositório (não é uma lista universal — é
só a lista que ESTE cenário específico pede hoje; um cenário diferente pediria outra):

| nome de fábrica | classe-base exigida | onde entra |
|---|---|---|
| `FlightState` | `base::ubf::AbstractState` | slot `state:` do agente de decisão |
| `BtBehavior` | `base::ubf::AbstractBehavior` | decisão do agente |
| `AltitudeSafetyBehavior` | `base::ubf::AbstractBehavior` | idem — exportado, hoje sem cenário que o instancie |
| `RLBridgeBehavior` | `base::ubf::AbstractBehavior` | idem — decisão vem de fora (ponte RL) |
| `FlightAction` | `base::ubf::AbstractAction` | devolvida pela decisão, executa no player |
| `AlertDatalink` | `models::Datalink` | achada por tipo entre os componentes do player |
| `TacticalAlert` | `base::Object` | carga útil transportada pelo alerta acima |
| `FlightAgentTC` | `base::ubf::AgentTC` | o agente — decide na fase 3 do frame de tempo crítico |
| `ThreadTagProbe` | `base::Component` | publica qual thread do pool T/C processou o player |

Se o seu modelo for carregado por um cenário diferente, a lista de nomes/slots que importa é a
`provides:` **daquele** cenário — releia-o antes de decidir o que implementar.

## 3. Publicar o que o modelo decidiu — a obrigação que falha em silêncio

O host precisa mostrar, na tela e nos arquivos que exporta, o que cada player está fazendo
**agora** — mas quem sabe isso é só o modelo (o host não entende o vocabulário de decisão dele).
A ponte é uma estrutura compartilhada e protegida por mutex, `mixr::xboard::Readout`, indexada por
id de player: o modelo escreve nela no momento em que decide/atua, o host só lê.

```cpp
#include "xboard/Board.hpp"

xboard::setBehaviorLabel(playerId, "PATROL");             // rotulo do que foi escolhido
xboard::bumpDecisionCount(playerId);                       // uma decisao de fato ATUADA
xboard::setAlert(playerId, valido, remetente, contato);    // se o modelo tiver alerta tatico
xboard::setDatalinkCounters(playerId, enviados, recebidos);
xboard::setThreadTag(playerId, tag);                        // se decidir num pool de threads
xboard::setRadarScan(playerId, achou, az, el, alcance, feixeH, feixeV);  // percepcao por sensor
```

**Nada no empacotamento obriga isso.** Um modelo que nunca chame essas funções compila, carrega,
satisfaz `provides:`, e o host sobe e roda — só que a tela de status mostra `bt=--`, a contagem de
decisões fica em `0`, e nenhum alerta aparece, para sempre, sem nenhum erro em lugar nenhum. É a
única obrigação desta lista que não tem sintoma de falha visível — por isso vale destacar aqui, e
não só nos comentários do header.

O radar é o caso mais claro de "isto é do modelo, não do host": só quem decide para onde apontar o
sensor sabe o que ele está enxergando. Um modelo sem sensor nenhum é legítimo — nesse caso,
simplesmente não chame `setRadarScan`, e o campo correspondente fica marcado como inválido em vez
de mostrar um valor inventado.

## 4. O `.so` sozinho pode não ser a entrega completa

Se o seu modelo referencia um arquivo próprio (por exemplo, uma árvore de decisão descrita em
XML, apontada por um slot do tipo `treeFile:`), esse arquivo é dado **do modelo**, não do cenário
que o carrega — publique-o junto do `.so` (via `install_data()`/`install_subdir()` no
`meson.build`) e faça o cenário apontar para onde ele foi instalado.

## 5. O que NÃO é exigido

Nada aqui obriga uma arquitetura interna específica: nenhuma árvore de comportamento, nenhum
framework de decisão em particular, nenhuma separação de camadas. `mirror.cpp` decide com uma
linha de código e não linka nenhuma biblioteca de árvore de comportamento — só o necessário para
satisfazer as seções 1 a 4.

## 6. Nomear certo: namespace aninhado sob `mixr::models::x<nome>`

Esta seção é sobre RISCO DE RUNTIME, não sobre estilo, e — como a seção 3 — não é imposta pelo
compilador nem por guarda nenhuma hoje: se ignorada, o sintoma aparece longe do lugar do erro.

Um cenário PODERIA, em tese, carregar mais de um `.so` de modelo no mesmo processo (nenhum cenário
de produção deste repositório faz isso hoje). Os `.so` são abertos com `RTLD_LOCAL`, e sob esse
modo a comparação de `type_info` deste toolchain degrada para `strcmp` do nome *mangled* — dois
tipos DIFERENTES com o MESMO nome qualificado, em dois `.so` distintos, colidiriam
silenciosamente.

Por isso, todo modelo NOVO (qualquer coisa além dos dois artefatos deste diretório, que já seguem
isto — `xtemplate` para o scaffold copiável, `xtemplate_mirror` para o mirror de contrato, nunca
compartilhando namespace) precisa aninhar TODO o próprio namespace — incluindo um eventual
`domain::` — dentro de `mixr::models::x<nome-do-modelo>`, nunca solto. `models/players/A-4`
(`domain::` solto no global) é a exceção histórica, cara demais para corrigir agora — **não** é o
exemplo a copiar. O raciocínio completo (por que o RTTI degrada, por que aninhar resolve) está em
[`ARCHITECTURE.md`](ARCHITECTURE.md), seção "Por que `domain::` mora DENTRO de...".

A colisão de NOME DE FÁBRICA (o string que o `.edl` usa em `provides:`, não o namespace C++) é
prima deste problema e já tem guarda automática —
[`../../../../tests/guard/check_colisao_fabrica.py`](../../../../tests/guard/check_colisao_fabrica.py)
— que já pegou um caso real em produção (`ThreadTagProbe` → `MissileThreadTagProbe`, do extinto
modelo `missile` — ver `CLAUDE.md` raiz). A colisão de `type_info` desta seção é mais difícil de
flagrar automaticamente — por isso o namespace aninhado é a defesa que não depende de um teste
pegar o problema depois.

## Limites do que este documento garante

- A tabela da seção 2 é um retrato do cenário de produção **de hoje**; ela muda se o cenário
  mudar. O que não muda é o mecanismo: sempre é o `.edl` que manda.
- Nada aqui é verificado pelo compilador — a única forma de conferir é rodar o modelo dentro do
  host de verdade. Uma interface abstrata em C++ resolveria isso, mas traria de volta o
  acoplamento binário (vtable) que este mecanismo de plugin existe justamente para evitar.

## Para ir além deste documento

- [`../src/mirror.cpp`](../src/mirror.cpp) — a implementação de referência, item por item desta
  lista (não é o scaffold copiável — ver o aviso no topo do próprio arquivo)
- [`../README.md`](../README.md) — como compilar e testar este diretório sozinho
- [`libs/xplugin/PluginAbi.hpp`](../../../../libs/xplugin/PluginAbi.hpp) — o contrato de
  empacotamento binário completo
- [`libs/xboard/Board.hpp`](../../../../libs/xboard/Board.hpp) — todos os campos e funções do
  quadro de leitura da seção 3
- [`../../../../CONTRIBUTING.md`](../../../../CONTRIBUTING.md) — como registrar um modelo novo
  num cenário
- [`../../../../CLAUDE.md`](../../../../CLAUDE.md), seção "O MODELO é um plugin, construído
  numa etapa PRÉVIA" — visão geral de como os modelos deste repositório se encaixam no host
