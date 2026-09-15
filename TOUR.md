# TOUR.md — tour guiado por este repositório

Este documento não explica nada por si — é uma **trilha**: em que ordem visitar o que já existe,
e em qual documento procurar a resposta em cada parada. Ele existe porque montar essa sequência
hoje exige conhecimento tribal do repositório; a ideia é que deixe de exigir.

Duas formas de usar:

1. **Chegando agora, por conta própria.** Siga a "Trilha principal" e depois a "Trilha de
   aprofundamento", na ordem.
2. **Fazendo o Teste Cego de Onboarding** (o exercício de usabilidade de documentação). Use as trilhas abaixo como checklist, respeitando as regras da seção "Se você está fazendo o
   Teste Cego de Onboarding".

## Antes de começar

- **Não é preciso credencial de nenhum *remote* privado** (*remote*, no jargão do Conan — o
  gerenciador de pacotes deste projeto —, é um repositório de onde pacotes binários são baixados;
  o ConanCenter público é um exemplo, ver `README.md`, seção "Pré-requisitos"). `README.md` e
  `INSTALL.md` §4 já documentam um caminho 100% sem credencial (`./scripts/deps.sh`) — é o mesmo
  que o CI usa. Se em algum ponto você sentir que precisa "pedir acesso a alguém", pare e releia
  essa seção antes de travar o exercício nisso.
- **`CLAUDE.md` não é material de leitura para este tour.** É um diário de arquitetura mantido
  para dar contexto a sessões de programação agêntica (Claude Code), escrito em ordem cronológica
  de decisão, não pedagógica — ele mesmo diz isso logo no topo. Nenhuma parada abaixo aponta pra
  ele; se outro documento te levar até lá no meio do caminho, trate como consulta pontual a uma
  seção específica, nunca como leitura de fundo.
- **Tacview é aplicativo de terceiro.** Não vem com o repositório — e o critério de sucesso do
  item 2 ("Tacview conectado, telemetria chegando") exige especificamente a edição **Advanced**
  (paga): a Standard/gratuita não recebe conexão ao vivo, só abre depois um `.acmi` já gravado
  (**ACMI** — *Air Combat Maneuvering Instrumentation*, o formato de gravação/streaming nativo do
  Tacview; ver "Pré-requisitos" no `README.md`). Faltando o Tacview, ou faltando essa edição, isso
  é um bloqueio de ambiente para o item 2, não de documentação.
- **Tempo aproximado**: trilha principal, meio dia (a maior parte é o primeiro build); trilha de
  aprofundamento inteira, mais um dia — dá para dividir em sessões, ver "Se você está fazendo o
  Teste Cego de Onboarding" abaixo.

## Trilha principal (essencial)

### 1. Subir o ambiente

**Objetivo:** ter `./dist/bin/app` funcionando.
**Onde procurar:** `README.md`, seções "Pré-requisitos" e "Build"; `INSTALL.md` do início até §4 é
passo prévio esperado (não só socorro pontual) se você não tem credencial de nenhum remote Conan
privado. `INSTALL.md` §1–§3 instalam os pacotes de sistema, o Conan (via `pipx`) e o perfil
default — sem isso, o `./scripts/deps.sh` de §4 nem roda. É o `deps.sh` que builda
`mixr`/`behaviortree.cpp.asa` do fonte — e é deles que o primeiro `make configure` depende.
`make run-app`, usado no critério de sucesso abaixo, é documentado na seção "Rodar" do
`README.md` — a mesma seção "Rodar" (não o `make run-app` em si) é revisitada no item 2 a
seguir, já com outro comando (`-folder`).
**Critério de sucesso:** `make configure && make models && make install` (a rotina do dia a dia,
ver "Build" no `README.md`) completa sem erro, e `make run-app` abre a tela de seleção de cenário.

### 2. Rodar um cenário e visualizar no Tacview

**Objetivo:** ver pelo menos uma aeronave se movendo e trocando de comportamento, ao vivo, no
Tacview.
**Onde procurar:** seção "Rodar" do `README.md` para o comando; o `README.md` do subprojeto
escolhido (ex.: `src/poc/dis/`) para a porta (o "callsign do handshake" listado ali é só um
detalhe interno do protocolo Tacview, não algo que se configura no visualizador — **handshake** é
o texto de abertura que o Tacview exige antes de aceitar qualquer dado; a definição completa, com
o `callsign` que cada poc anuncia, está em `src/poc/dis/README.md`) — e para o aviso de que
`flight` sozinho não basta: sem o intruso, não há o que evadir. `src/poc/dis/README.md` já traz o
comando das duas em dois terminais (`bandit` + `flight`); é isso que produz a manobra do critério
de sucesso abaixo.
**Critério de sucesso:** Tacview conectado, telemetria chegando, pelo menos uma manobra de troca
de comportamento visível na trajetória (evasão, alerta, apoio — a cadeia citada na seção "Rodar"
do `README.md`). O rótulo textual desse comportamento
(`EVADE` — a mesma manobra citada em minúsculo, "evade", na tabela da seção "Rodar" do
`README.md`) não aparece dentro do Tacview — só no status/TUI do `./app`, ver item 3 a seguir.

### 3. Explorar o `app` (o painel de controle / **TUI** — *Text User Interface*, interface de
texto interativa em terminal)

**Objetivo:** navegar pelas abas do painel enquanto uma simulação roda (Players, Mapa, Memória,
Tempo Não-Crítico, Log, Componentes, EDL — sigla já definida no parágrafo de abertura do
`README.md`, item 1).
**Onde procurar:** `app/README.md`.
**Critério de sucesso:** identificar em qual thread cada player está decidindo, pausar/acelerar o
tempo, e abrir o card de detalhe de uma entidade.

### 4. Abrir o manual interativo

**Objetivo:** usar o manual interativo — cinco visões sobre o framework, entre elas o ciclo de
execução do MIXR, a cadeia de decisão e o catálogo de classes.
**Onde procurar:** seção "Leia mais" do `README.md`, linha `docs/manual/`; `make open-docs`.
**Critério de sucesso:** localizar, na aba de comportamento, a cadeia de decisão real de produção
`FlightAgentTC` (o agente que decide dentro do frame de tempo crítico, componente do próprio
`Player`) `→ Agent::controller → BtBehavior → flight_tree.xml → FlightAction` — a mesma cadeia
descrita em `docs/manual/README.md`.

## Trilha de aprofundamento (se sobrar tempo)

### 5. Editor gráfico de cenário (`edl-builder`)

**Objetivo:** montar um `.edl` (**EDL** — *English Description Language*, a linguagem de
configuração declarativa do MIXR: descreve cenário, *players*, sensores e rede; definição completa
no `README.md`, parágrafo de abertura) do zero arrastando classes de uma paleta, exportar, e
validar o resultado.
**Onde procurar:** `src/ui/README.md`; `make open-edl`.
**Critério de sucesso:** exportar um `.edl` que passa sem erro em `./dist/bin/edlcheck <arquivo>`.
Atalho para ver algo funcionando rápido: botão "Carregar preset" — mas esse cenário de exemplo tem
um placeholder `@NUM_TC_THREADS@` não resolvido (ver a aba "Abertos" antes de exportar), então
exportar direto sem preenchê-lo falha no `edlcheck` de propósito, não é bug do preset.

### 6. Editar/monitorar uma árvore de comportamento com o Groot

**Objetivo:** abrir uma árvore de comportamento de produção (`models/players/A-4/configs/flight_tree.xml`) no **Groot**
(editor/monitor visual de árvores de comportamento do BehaviorTree.CPP, aplicativo de terceiro —
não vem com o repositório).
**Onde procurar:** `CONTRIBUTING.md`, seção "Editando e depurando a árvore com o Groot" (dentro
do passo 4 do roteiro de modelo novo — role até lá, ou busque pelo título; a instalação do Groot
em si é `INSTALL.md` §4).
**Critério de sucesso:** a árvore abre sem erro de "modelo não registrado" e dá para editar/salvar.

### 7. Rodar a poc com decisão em Python

**Objetivo:** rodar `python-flight` e editar um script de decisão vivo, sem recompilar nada.
**Onde procurar:** `src/poc/python-flight/README.md`.
**Critério de sucesso:** uma mudança num arquivo `.py` de política muda o comportamento observável
na próxima execução.

### 8. `src/rl` — o ambiente de RL

**Objetivo:** entender o wrapper Gymnasium e rodar o smoke test contra a simulação de verdade.
**Onde procurar:** `src/rl/README.md`.
**Critério de sucesso:** o teste de fumaça do ambiente passa.

### 9. `src/poc/rl-training` — treinar uma política de verdade

**Objetivo:** treinar uma política via PPO (*Proximal Policy Optimization*, o algoritmo de RL que
o **Stable-Baselines3** — biblioteca Python de algoritmos de RL — implementa) e exportar para
`.onnx` (o formato do **ONNX** — *Open Neural
Network Exchange* — para redes treinadas).
**Onde procurar:** `src/poc/rl-training/README.md`; para rodar o `.onnx` gerado na poc de verdade
(portas, armadilhas confirmadas), `src/poc/onnx-policy/README.md`.
**Critério de sucesso:** um `.onnx` novo gerado, carregável pela poc `onnx-policy` — carregar sem
erro é o critério aqui, não voar de forma equivalente ao `.onnx` de referência já versionado (sem
*reward shaping*, o treino PPO deste item aprende a maximizar recompensa, não a reproduzir
nenhuma regra específica; ver `src/poc/onnx-policy/README.md` para o contraste com o caminho
rápido de clonagem de comportamento). Sem exceção de time-box própria como o item 1 — o treino
PPO (200 mil passos, física via JSBSim) pode facilmente estourar os 45 min sugeridos; se isso
acontecer, deixe rodando em background e anote o bloqueio, mesma orientação do item 1.

### 10. Criar um modelo novo como plugin

**Objetivo:** o item mais pesado do tour — sair do zero, gerar o scaffold, e ver seu próprio `.so`
carregado por um cenário.
**Onde procurar:** `CONTRIBUTING.md` (ponto de entrada declarado); `models/REGISTRO.md` antes de
começar, para não duplicar trabalho de outra pessoa.
**Critério de sucesso:** `make new-model NAME=<nome> CATEGORY=player` gera o projeto, ele compila
(`make models`) e um cenário próprio carrega o `.so` resultante.

## Se você está fazendo o Teste Cego de Onboarding

Regras que tornam o relatório final útil em vez de anedota:

- **Registre em tempo real, não de memória no fim.** A cada travamento: horário, o que tentou, o
  que esperava, o que aconteceu, quanto tempo perdeu. Memória de "o que foi confuso" degrada
  rápido — principalmente depois que você já resolveu e o caminho parece óbvio em retrospecto.
- **Time-box por item**: ~30 min nos itens da trilha principal, ~45 min nos de aprofundamento —
  **exceto o item 1**, cujo tempo de build (desde o `./scripts/deps.sh` de `INSTALL.md` §4, quando
  aplicável, até `make install`) não conta contra o box: é o próprio "Antes de começar" que avisa
  que a trilha principal leva meio dia por causa dele. Deixe o build rodando, anote o horário de
  início/fim à parte, e só aplique o time-box de
  30 min ao que sobrar de esforço ativo (ler a seção, digitar o comando, investigar um erro). Nos
  demais itens, estourou o tempo, anote exatamente onde travou e siga para o próximo — não afunde
  um item inteiro e deixe de visitar o resto (mas repare que o item 2 depende do binário que o
  item 1 produz: se o item 1 não terminou, "seguir para o próximo" significa anotar o bloqueio e
  esperar o build, não pular a etapa).
- **Cite a seção exata** (do README, do `.edl`, do documento que for) onde a dúvida aconteceu —
  o projeto já é fortemente referenciado por seção, custa pouco anotar e vira ação editável
  direto.
- **Pode me perguntar só bloqueio de infraestrutura/acesso** (ex.: falta o Tacview instalado,
  driver de dispositivo, credencial). Evite perguntar dúvida de "como eu faço X" — é exatamente o
  que o exercício quer medir.
- **Ao final, me responda:** o que foi mal explicado, o que faltou explicar, o que nem chegou a
  virar dúvida (você só percebeu que existia depois), quais pontos do projeto você não visitou,
  quais comandos quebraram, sugestões de melhoria sobre o que já existe, e o que seria muito bom
  se tivesse.
