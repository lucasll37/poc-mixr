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

- **Não é preciso credencial de nenhum remote privado.** `README.md` e `INSTALL.md` §4 já
  documentam um caminho 100% sem credencial (`./scripts/deps.sh`) — é o mesmo que o CI usa. Se em
  algum ponto você sentir que precisa "pedir acesso a alguém", pare e releia essa seção antes de
  travar o exercício nisso.
- **`CLAUDE.md` não é material de leitura para este tour.** É um diário de arquitetura mantido
  para dar contexto a sessões de programação agêntica (Claude Code), escrito em ordem cronológica
  de decisão, não pedagógica — ele mesmo diz isso logo no topo. Nenhuma parada abaixo aponta pra
  ele; se outro documento te levar até lá no meio do caminho, trate como consulta pontual a uma
  seção específica, nunca como leitura de fundo.
- **Tacview é aplicativo de terceiro.** Não vem com o repositório; se você não tiver o Tacview
  instalado, isso é um bloqueio de ambiente para o item 2, não de documentação.
- **Tempo aproximado**: trilha principal, meio dia (a maior parte é o primeiro build); trilha de
  aprofundamento inteira, mais um dia — dá para dividir em sessões, ver "Se você está fazendo o
  Teste Cego de Onboarding" abaixo.

## Trilha principal (essencial)

### 1. Subir o ambiente

**Objetivo:** ter `./dist/bin/app` funcionando.
**Onde procurar:** `README.md`, seções "Pré-requisitos" e "Build"; `INSTALL.md` para qualquer
ferramenta de sistema faltando.
**Critério de sucesso:** `make configure && make sdk && make models && make build && make install` completa sem
erro, e `make run-app` abre a tela de seleção de cenário.

### 2. Rodar um cenário e visualizar no Tacview

**Objetivo:** ver pelo menos uma aeronave se movendo e trocando de comportamento, ao vivo, no
Tacview.
**Onde procurar:** seção "Rodar" do `README.md` para o comando; o `README.md` do subprojeto
escolhido (ex.: `src/poc/dis/`) para a porta/callsign que o Tacview espera.
**Critério de sucesso:** Tacview conectado, telemetria chegando, pelo menos uma troca de rótulo de
comportamento visível.

### 3. Explorar o `app` (o painel de controle / TUI)

**Objetivo:** navegar pelas abas do painel enquanto uma simulação roda (Players, Mapa, Memória,
Fundo, Log, Componentes, EDL).
**Onde procurar:** `app/README.md`.
**Critério de sucesso:** identificar em qual thread cada player está decidindo, pausar/acelerar o
tempo, e abrir o card de detalhe de uma entidade.

### 4. Abrir o manual interativo

**Objetivo:** usar o visualizador do ciclo de execução do MIXR e o catálogo de classes.
**Onde procurar:** `README.md` → linha do `docs/manual/`; `make open-docs`.
**Critério de sucesso:** localizar, na aba de comportamento, a cadeia
`FlightAgentTC → UbfArbiter → árvore de comportamento`.

## Trilha de aprofundamento (se sobrar tempo)

### 5. Editor gráfico de cenário (`edl-builder`)

**Objetivo:** montar um `.edl` do zero arrastando classes de uma paleta, exportar, e validar o
resultado.
**Onde procurar:** `src/ui/README.md`; `make open-edl-builder`.
**Critério de sucesso:** exportar um `.edl` que passa sem erro em `./dist/bin/edlcheck <arquivo>`.

### 6. Editar/monitorar uma árvore de comportamento com o Groot

**Objetivo:** abrir uma árvore de comportamento de produção (`models/players/A-4/configs/flight_tree.xml`) no editor visual.
**Onde procurar:** `CONTRIBUTING.md` (a instalação do Groot em si é `INSTALL.md` §4).
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

**Objetivo:** treinar (ou clonar comportamento) e exportar uma política para `.onnx`.
**Onde procurar:** `src/poc/rl-training/README.md`.
**Critério de sucesso:** um `.onnx` novo gerado, carregável pela poc `onnx-policy`.

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
- **Time-box por item**: ~30 min nos itens da trilha principal, ~45 min nos de aprofundamento.
  Estourou o tempo, anote exatamente onde travou e siga para o próximo — não afunde um item
  inteiro e deixe de visitar o resto.
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
