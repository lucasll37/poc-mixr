# `app` — painel de controle e monitoramento (TUI)

Aplicação de terminal estilizada (cores, navegação por teclado e por mouse, redesenho
responsivo — biblioteca [FTXUI](https://github.com/ArthurSonzogni/FTXUI)) para **carregar,
acompanhar e controlar** uma simulação, em vez de ler uma linha de status em texto puro.

Roda a **mesma pilha nativa** de [`src/multi-thread/`](../src/multi-thread/) — mesmo plugin de
modelo (`libflight_tc.so`), mesmo `Aircraft`/`JSBSimModel`/`Autopilot`/radar/`FlightAgentTC`
(os 4 falcons decidem em PARALELO, um por thread do pool de tempo crítico, na fase 3 do frame),
nenhuma mudança em [`models/`](../models/) — só troca a impressão de status por este painel. Mora fora de
`src/` porque não é "mais uma poc" (nunca troca DIS com as outras, não é uma variação de "onde a
decisão roda"): é a ferramenta de controle/monitoramento das demais, por isso tem pasta própria na
raiz e o alvo/binário se chamam `app`, não `dashboard`.

> Convenção do repositório: esta documentação é em português do Brasil; identificadores, nomes de
> slot e rótulos que vêm do modelo (`PATROL`, `EVADE`, nomes de nó da árvore…) ficam como estão.

---

## Índice

1. [Início rápido](#1-início-rápido)
2. [Cenários](#2-cenários)
3. [Linha de comando](#3-linha-de-comando)
4. [A interface — quatro abas](#4-a-interface--quatro-abas)
5. [Aba Players](#5-aba-players)
6. [Aba Mapa](#6-aba-mapa)
7. [Aba Memória](#7-aba-memória)
8. [Aba Fundo](#8-aba-fundo)
9. [Árvore de comportamento e breakpoints](#9-árvore-de-comportamento-e-breakpoints)
10. [Todos os atalhos de teclado](#10-todos-os-atalhos-de-teclado)
11. [Todos os elementos clicáveis](#11-todos-os-elementos-clicáveis)
12. [Ações disruptivas e confirmação](#12-ações-disruptivas-e-confirmação)
13. [Portas, arquivos e dados](#13-portas-arquivos-e-dados)
14. [Arquitetura interna, em resumo](#14-arquitetura-interna-em-resumo)
15. [Limitações conhecidas](#15-limitações-conhecidas)
16. [Onde ler mais](#16-onde-ler-mais)

---

## 1. Início rápido

```bash
make configure && make build   # a partir da raiz do repositório — ver README.md raiz
make run-app                   # sem argumento nenhum: mostra a tela de seleção de cenário
```

Ou direto pelo binário (sempre a partir da **raiz** do repositório — `configs/`/`data/` são lidos
por caminho relativo):

```bash
./build/app/src/app                        # tela de seleção
./build/app/src/app -scenario intercept    # pula direto pro cenário "Intercepto"
```

Não precisa de nenhum outro processo rodando — os três cenários são **herméticos** (sem
`networks:`) e usam porta de Tacview e diretório de dados **próprios**, então dá para rodar junto
com `single-thread`/`multi-thread`/`bandit-dis` sem colidir (ver [§13](#13-portas-arquivos-e-dados)).

---

## 2. Cenários

A tela de seleção (sem `-scenario`) lista os três; `-scenario <chave>` pula direto para um deles.

| chave | rótulo | conteúdo |
|---|---|---|
| `patrol` | Patrulha | 4 falcons patrulhando, sem intruso — bom para ver pausar/acelerar sem ruído |
| `intercept` | Intercepto | + `bandit1` local — mostra evasão e apoio entre os falcons (`EVADE`/`SUPPORT`) |
| `intercept_missile` | Intercepto + Míssil | + `falcon1` com um míssil guiado — lançamento/detonação, ótimo para pausar bem no meio (ver "Demo: míssil guiado" no [CLAUDE.md](../CLAUDE.md)) |

Cada cenário tem seu próprio arquivo em [`configs/`](configs/) (`scenario_<chave>.epp.in`),
expandido em tempo de execução para `configs/<chave>.generated.epp` (gitignored).

---

## 3. Linha de comando

| opção | efeito |
|---|---|
| `-scenario <chave>` | pula a tela de seleção e carrega o cenário direto (`patrol`, `intercept` ou `intercept_missile`) |
| `-threads <N>` | força `numTcThreads` do pool nativo de tempo crítico (sem isso: detecta `hardware_concurrency()`, limitado a 8) |
| `-deterministic <N>` | roda **N frames de passo fixo** e sai — sem TUI, sem TTY; imprime linhas `frame=` e o relatório de instâncias no final (ver [tests/scenario/run_app_test.py](../tests/scenario/run_app_test.py)) |
| `-parallel-decision` | só faz sentido junto com `-deterministic`: decide os 4 players em paralelo em vez de sequência (ver [`app/DeterministicRun.hpp`](include/app/DeterministicRun.hpp)) |

`-deterministic` é o caminho usado pela suíte automatizada (`make test`, suíte `scenario-app-*`) —
não abre terminal interativo, então funciona em CI.

---

## 4. A interface — quatro abas

```
╭──────────────────────────────────────────────────────────────────────╮
│ app  <cenário>  entidades=N                t=Xs  sim=Ys  <vel>  thr=N │
╰──────────────────────────────────────────────────────────────────────╯
[F1] Players  [F2] Mapa  [F3] Memória  [F4] Fundo
──────────────────────────────────────────────────────────────────────
                        (conteúdo da aba ativa)
──────────────────────────────────────────────────────────────────────
 [+] Acelerar  [-] Frear  [espaço] Pausar  [1] Tempo real  [m] Ver no mapa      [l] Carregar  [r] Reiniciar  [s] Parar  [q] Sair
```

O cabeçalho mostra o cenário carregado, quantas entidades existem agora, tempo de parede (`t=`) e
tempo simulado (`sim=`) lado a lado, a velocidade atual (ou `PAUSADO`) e quantas threads de tempo
crítico o pool tem. As quatro abas e a barra de ações (`Acelerar`/`Frear`/…) ficam **sempre
visíveis**, qualquer que seja a aba ativa — trocar de aba não perde o controle de tempo nem as
ações disruptivas.

Clique em `[F1]`/`[F2]`/`[F3]`/`[F4]` ou use as teclas de função para trocar de aba.

---

## 5. Aba Players

Lista rolável (rola sozinha quando não cabe tudo na tela) com uma linha por entidade do cenário —
**qualquer** tipo de player, não só aviões (ver [§14](#14-arquitetura-interna-em-resumo)) — mais o
card de detalhe da entidade selecionada, ao lado. O cabeçalho da lista nomeia cada coluna.

**Colunas da lista:**

| coluna | conteúdo |
|---|---|
| (glifo) | tipo de player — `A` avião, `G` veículo terrestre, `S` navio, `W` arma, `X` nave espacial, `B` construção, `L` forma de vida |
| nome | o nome do player no cenário (`falcon1`, `bandit1`, `W10001`…) |
| tipo | a string `type:` do EDL, ou o nome da classe C++ quando o modelo não declara uma |
| comportamento | o rótulo da folha da árvore de comportamento vencedora agora (`PATROL`, `EVADE`…), ou `--` se a entidade não publica decisão |
| thread | qual thread do pool de tempo crítico decidiu (`T0`, `T1`…), ou `-` quando não se aplica — cada falcon decide numa thread diferente do pool (a decisão roda na fase 3 do frame de tempo crítico, não no laço de background); `bandit1` fica sempre em `-` porque não tem agente de decisão local |
| altitude, velocidade, combustível | os de sempre — combustível só aparece pra quem tem (aeronaves) |

**Card de detalhe** (clique numa linha da lista, ou use as setas ↑/↓): posição, atitude,
velocidade/Mach, G e empuxo (quando fazem sentido para o tipo), medidor de combustível, pista de
radar mais próxima, alerta tático recebido, contagem de decisões — e, se a entidade tem árvore de
comportamento, a própria árvore (ver [§9](#9-árvore-de-comportamento-e-breakpoints)). A thread
que decidiu fica só na lista, não se repete aqui.

---

## 6. Aba Mapa

Vista navegável da simulação, num canvas de alta resolução (caracteres braille).

### Duas perspectivas (`[v]` ou botão "Vista")

| perspectiva | plano | unidade dos eixos |
|---|---|---|
| **Cima** (padrão) | Norte/Leste, olhando de cima | **milhas náuticas** (x/y), com barra de escala explícita |
| **Lado** | largura × altitude | pés no eixo Y — com **piso em -1000 ft**: a grade não desce além disso, e uma linha vermelha marca o limite |

Nas duas, o eixo "livre" (em torno do qual dá para girar) é o vertical — `[,]`/`[.]` giram a
visualização (rosa dos ventos na vista de Cima; de que rumo você está olhando a formação, na
vista de Lado). Os eixos são sempre relativos ao ponto que está centralizado agora (o "pan"), não
ao referencial absoluto do mundo — por isso continuam fazendo sentido depois de arrastar/girar.

### Navegação

- **Arrastar** (botão esquerdo do mouse, dentro do canvas): move o mapa.
- **Setas do teclado**: também movem (mesmo efeito do arrasto).
- **`[`/`]`** ou **roda do mouse**: zoom.
- **`[c]`**: centraliza na entidade selecionada.
- **`[t]`**: liga/desliga o rastro (trilha das últimas posições) de cada entidade.
- **`[e]`**: liga/desliga a vista de terreno (elevação) — ver abaixo.

### Vista de terreno (`[e]` ou botão "Terreno")

Desligada por padrão.

| perspectiva | o que aparece |
|---|---|
| **Cima** | **curvas de nível**, sem cor (cinza), finas — uma linha a cada intervalo de elevação "redondo" (5/10/20/50/100 m…, escolhido sozinho conforme o alcance visível e espaçado o suficiente pra não poluir a tela), como um mapa topográfico |
| **Lado** | uma **linha de contorno** do chão, sempre em **vermelho** — o perfil de elevação ao longo da linha de visada que passa pelo ponto centralizado, como silhueta ABERTA (não preenchida). É essa linha que marca "onde é o chão" nesta vista — não existe mais um piso fixo de -1000 ft |

**Cobertura**: a vista de terreno não está mais presa ao único tile que o cenário declara em
EDL — carrega **todos** os tiles `.hgt` encontrados em `shared/data/terrain/srtm/` e escolhe o
certo por coordenada a cada consulta. Hoje isso cobre uma área de 2°×2° ao redor do cenário
(Serra do Mar, RJ): o tile real (`S23W043`) mais três vizinhos **sintéticos** (elevação gerada
por uma função matemática simples, não dado real — ver
[`shared/data/terrain/srtm/README.md`](../shared/data/terrain/srtm/README.md) para o porquê e
como acrescentar tiles reais depois, sem mudar nenhum código). Fora de toda cobertura carregada,
`[e]` simplesmente não desenha nada ali — degrada em silêncio, sem inventar elevação. Uma célula
"void" (buraco na cobertura de radar original, sem dado — não deve acontecer nos tiles deste
repositório, mas pode acontecer num tile de terceiro) também é tratada como "sem dado", nunca
desenhada como se fosse uma elevação real.

**Enquadramento automático (só na perspectiva Lado)**: ao ligar `[e]`, trocar pra vista `[v]`
Lado, ou centralizar `[c]` numa entidade, a referência vertical é reancorada pra que o nível do
terreno **no ponto centralizado** apareça perto do limite inferior da janela — em vez de
flutuar em qualquer altura (ou sumir da tela, dependendo do zoom). É um ajuste PONTUAL, feito só
nesses três momentos — depois disso as setas continuam movendo o pan livremente, sem nenhum
recentramento automático atrapalhando.

### O que aparece desenhado

Cada entidade é uma **bolinha** colorida pelo lado (`BLUE`/`RED`/`YELLOW`/`CYAN`/`GRAY`/`WHITE`),
com uma **linha** saindo dela na direção do rumo atual (só quem tem dinâmica de movimento — tudo
exceto construções) e o **nome**, sem moldura ao redor, ligado ao ponto por uma linha guia.
**Clicar** numa entidade a seleciona — o mesmo efeito de selecionar na aba Players, então o card
de detalhe ao lado (e a árvore de comportamento nele) atualiza para ela.

---

## 7. Aba Memória

Uma linha por classe do MIXR amostrada ao vivo (10 Hz) — cobre automaticamente **qualquer**
modelo carregado (as classes vêm do próprio plugin, via `xplugin::pluginMetaObjects()`), mais duas
classes do host como termômetro geral do parser (`Pair`, `String`).

| coluna | significado |
|---|---|
| origem | `H` (host) ou `P` (plugin — o modelo carregado) |
| classe | o nome de fábrica |
| `count=` | instâncias **vivas agora** |
| `pico=` | o maior `count` já visto nesta execução |
| `criados=` | quantas instâncias já foram construídas, do início até agora |
| barra | `count`/`pico` — como `pico` só cresce (nunca encolhe), a escala da barra se ajusta sozinha com o tempo |
| `CRESCENDO` | aparece quando `count` **nunca caiu** numa janela de ~3 s e termina maior que começou — sinal de possível vazamento |

Use para investigar vazamento de memória ao vivo, sem esperar o processo terminar — é a mesma
pergunta que `app/MetaObjectReport` já respondia no final de uma execução `-deterministic`, só que
contínua.

---

## 8. Aba Fundo

Painel estático (sem lista) com o que roda na **thread de tempo NÃO crítico** — este `app` nunca
cria a `StationBgPeriodicThread` nativa do MIXR; quem faz esse papel é o próprio laço que atualiza
a interface (o mesmo que publica o `DashboardState` a cada amostra), chamando
`station->updateData(dt)` fora do frame de tempo crítico.

| seção | conteúdo |
|---|---|
| Laço de atualização | taxa alvo (10 Hz) vs. taxa **medida** de verdade, duração da última iteração, contador de iterações |
| Tacview / gravador | se a exportação está ligada, quantas varreduras de radar já foram publicadas |
| Terreno | se o banco de elevação (SRTM) está carregado |
| Rede (DIS) | quantos handlers de rede existem e a que taxa — os três cenários deste `app` são herméticos (sem `networks:`), então aparece "nenhum" |

É nesta mesma thread que o gravador é drenado para o Tacview, a elevação de terreno de cada
player é atualizada (`Player::updateElevation()`), a rede DIS seria processada se o cenário
declarasse `networks:`, e onde as outras três abas (Players/Mapa/Memória) são amostradas — a
taxa medida aqui é a mesma que rege quão "fresca" a UI inteira fica.

---

## 9. Árvore de comportamento e breakpoints

Quando a entidade selecionada tem árvore de comportamento (BehaviorTree.CPP), o card de detalhe
(nas duas abas, Players e Mapa) mostra a árvore inteira — lida do mesmo `treeFile:` que o cenário
já resolve, desenhada em estilo `tree` (linhas Unicode), com a folha que está **ganhando agora**
destacada em azul.

**A árvore é clicável** — clique numa folha (não num nó de controle como `Fallback`/`Sequence`)
para selecioná-la como alvo. Logo abaixo dela aparecem, conforme o estado:

| situação | o que aparece |
|---|---|
| nada selecionado ainda | dica pra clicar numa folha |
| selecionado um nó de controle | aviso pra escolher uma folha, não um nó de controle |
| uma folha selecionada | "Folha selecionada: …" + botões **[g] Rodar até aqui** / **[G] Rodar (máx. veloc.)** |
| breakpoint armado | "Aguardando: `<entidade>` → `<folha>` …" + botão **[x] Cancelar breakpoint** |
| atingido | "Breakpoint atingido: … — simulação PAUSADA" |
| não atingido a tempo | aviso de cancelamento automático (ver abaixo) |

**O que acontece ao armar** (`[g]` ou `[G]`, tecla ou botão): a simulação roda — na velocidade que
você já tinha escolhido (`[g]`) ou na **máxima possível**, sem limite de tempo real (`[G]`) — até
a entidade selecionada **no momento de armar** chegar naquela folha. Quando chega, a simulação
**pausa de verdade** (não é só a UI que "acha" que pausou).

**Se o nó nunca é atingido:** cancele a qualquer momento com `[x]`, ou deixe — depois de 300 s de
tempo **simulado** (não de parede: vale igual em 1× ou na velocidade máxima) sem atingir, o
breakpoint se desarma sozinho e avisa.

> O destaque da folha ativa (e o casamento nome-da-folha ↔ breakpoint) é por comparação de texto
> entre o nome do nó e o rótulo publicado pelo modelo — cobre a maioria dos casos, mas um nó cujo
> C++ decide em runtime entre rótulos sem relação textual com o próprio nome (ex.: a ação de
> evasão do modelo de produção decide entre `EVADE`/`BREAK` sem que nenhum dos dois apareça no
> nome do nó) não acende o destaque, mesmo sendo a folha certa. Ver [§15](#15-limitações-conhecidas).

---

## 10. Todos os atalhos de teclado

**Globais (qualquer aba):**

| tecla | ação |
|---|---|
| `+` / `=` | acelera o tempo |
| `-` / `_` | freia o tempo |
| espaço / `p` | pausa / retoma |
| `1` | volta ao tempo real (1×) |
| `F1` | aba Players |
| `F2` | aba Mapa |
| `F3` | aba Memória |
| `F4` | aba Fundo |
| `g` | roda até o breakpoint armado, na velocidade atual |
| `G` | roda até o breakpoint armado, na velocidade máxima |
| `x` | cancela o breakpoint armado |
| `l` | carregar outro cenário — **pede confirmação** |
| `r` | reiniciar o cenário atual — **pede confirmação** |
| `s` | parar (volta à seleção de cenário) — **pede confirmação** |
| `q` | sair — **pede confirmação** |
| `Ctrl+C` | sai imediatamente, sem confirmação (o FTXUI trata) |

**Só na aba Players:**

| tecla | ação |
|---|---|
| `↑` / `↓` | navega a lista de entidades |
| `m` | vai para a aba Mapa com a mesma entidade já selecionada |

**Só na aba Mapa:**

| tecla | ação |
|---|---|
| `↑`/`↓`/`←`/`→` | move o mapa (mesmo efeito de arrastar) |
| `[` / `]` | zoom |
| `,` / `.` | gira a visualização |
| `t` | liga/desliga o rastro |
| `e` | liga/desliga a vista de terreno |
| `v` | alterna a perspectiva (Cima/Lado) |
| `c` | centraliza na entidade selecionada |

**Só na aba Memória:**

| tecla | ação |
|---|---|
| `↑` / `↓` | navega a lista de classes |

**No diálogo de confirmação** (depois de `l`/`r`/`s`/`q`):

| tecla | ação |
|---|---|
| `Enter` | confirma a ação |
| `Escape` | cancela e volta pra UI normal |

---

## 11. Todos os elementos clicáveis

Além das teclas acima, **tudo tem um botão equivalente** — a dica do atalho já vem no rótulo do
próprio botão:

- As quatro abas: `[F1] Players`, `[F2] Mapa`, `[F3] Memória`, `[F4] Fundo`.
- Barra principal: `[+] Acelerar`, `[-] Frear`, `[espaço] Pausar`, `[1] Tempo real`,
  `[m] Ver no mapa` (só aparece com alguma entidade selecionada), `[l] Carregar`,
  `[r] Reiniciar`, `[s] Parar`, `[q] Sair`.
- Linha de comportamento na aba Players: clique seleciona a entidade.
- Canvas da aba Mapa: clique numa entidade a seleciona; arrastar move o mapa; roda do mouse dá
  zoom.
- Barra da aba Mapa: `[[] Zoom-`, `[]] Zoom+`, `[,] Girar<`, `[.] Girar>`, `[c] Centralizar`,
  `[t] Rastro: ON/OFF`, `[e] Terreno: ON/OFF`, `[v] Vista: Cima/Lado`.
- Linha de classe na aba Memória: clique seleciona.
- Linha da árvore de comportamento (nas duas abas): clique numa folha a seleciona para
  breakpoint; aparecem `[g] Rodar até aqui` / `[G] Rodar (máx. veloc.)` ou `[x] Cancelar
  breakpoint`, conforme o estado.
- Diálogo de confirmação: `[Enter] Confirmar`, `[Esc] Cancelar`.

---

## 12. Ações disruptivas e confirmação

`l` (carregar outro cenário), `r` (reiniciar), `s` (parar) e `q` (sair) **sempre pedem
confirmação** antes de executar — por tecla ou por clique, tanto faz. Enquanto o diálogo está
aberto, o resto da interface fica bloqueado (não dá pra clicar em nada por trás) até confirmar ou
cancelar.

As três primeiras (`l`/`r`/`s`) são, por baixo, a **mesma operação**: derrubar a `Station` atual
(`SHUTDOWN_EVENT` + `unref()`) e **reexecutar o próprio processo** (`execv`, via
[`app/Respawn.hpp`](include/app/Respawn.hpp)) — nunca uma segunda simulação dentro do mesmo
processo. `r` reexecuta com o mesmo `-scenario`; `l`/`s` reexecutam sem argumento, voltando à
tela de seleção. `q` só encerra.

---

## 13. Portas, arquivos e dados

| item | valor |
|---|---|
| Tacview (*Real-Time Telemetry*) | porta **1236** — diferente de `single-thread`/`multi-thread` (1234) e `bandit-dis` (1235), de propósito, pra rodar junto sem colidir |
| log | `./app/data/logs/app.log` |
| gravação Tacview | `./app/data/recordings/mission-<cenário>.acmi` |
| cenário expandido | `./app/configs/<chave>.generated.epp` (gitignored — gerado a cada carga) |
| terreno (SRTM) | `./shared/data/terrain/srtm/` — compartilhado com as outras pocs |

Todos os cenários são **herméticos** (sem bloco `networks:`) — não abrem porta DIS nenhuma, então
não competem com `bandit-dis` nem entre si.

---

## 14. Arquitetura interna, em resumo

`main.cpp` só orquestra, na ordem: tela de seleção (se precisar) → expande o `.epp.in` →
`StationBuilder` monta a `Station` → aplica o ajuste de manete de cruzeiro → roda
`DashboardLoop` (interativo) ou `DeterministicRun` (`-deterministic`) → desliga a `Station` →
`Respawn` se for o caso.

`app/DashboardLoop.cpp` é o laço de tempo real: **duas threads** — uma de simulação (10 Hz,
`station->updateData(dt)`, publica um `DashboardState` sob mutex a cada amostra) e a principal,
que só roda o laço do FTXUI (desenho + teclado + mouse). A captura de estado
(`app/DashboardState.cpp`) é **agnóstica ao tipo de player**: lê tudo pela base
`mixr::models::Player` — funciona para qualquer modelo carregado, não só o de produção, e
descobre entidades dinamicamente (`app::discoverPlayers`), então um míssil lançado em runtime
aparece sozinho, sem estar em nenhuma lista fixa.

Para o desenho completo — por que cada decisão foi tomada, as armadilhas encontradas rodando, os
detalhes de cada módulo — ver a seção `./app` do [CLAUDE.md](../CLAUDE.md) da raiz: é o registro
vivo de tudo que foi medido rodando, rodada por rodada.

---

## 15. Limitações conhecidas

- **Destaque de folha ativa e casamento de breakpoint são por texto**, não por um mapeamento
  formal — ver o aviso em [§9](#9-árvore-de-comportamento-e-breakpoints).
- **A árvore mostrada vem do PRIMEIRO `treeFile:` encontrado no cenário** — se um cenário
  hipotético desse árvores diferentes a players diferentes, todos veriam a mesma (não acontece em
  nenhum cenário deste repositório hoje).
- **`bandit1` sempre mostra `-` na coluna "thread"** — ele não tem `FlightAgentTC` local (é um
  intruso scriptado, só com `Autopilot`), então nunca decide; os quatro falcons, que decidem no
  pool de tempo crítico, mostram sua thread de verdade (`T0`, `T1`…).
- **`-deterministic` não roda o painel** — é o caminho de teste automatizado, sem TUI nem TTY;
  imprime texto puro (linhas `frame=` + relatório de instâncias).

---

## 16. Onde ler mais

| documento | responde |
|---|---|
| [CLAUDE.md](../CLAUDE.md), seção `./app` | o histórico completo de decisões e armadilhas, rodada por rodada |
| [README.md](../README.md) (raiz) | visão geral do repositório inteiro — os quatro subprojetos, os modelos como plugin, as bibliotecas compartilhadas |
| [models/README.md](../models/README.md) | como um modelo novo vira plugin, e o contrato que ele tem que cumprir |
| [tests/README.md](../tests/README.md) | a suíte automatizada — inclusive `scenario-app-*`, que roda os três cenários desta poc em `-deterministic` |
