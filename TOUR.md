# TOUR.md — tour guiado por este repositório

Trilha de onboarding: em que ordem visitar o que já existe, e onde procurar a resposta em cada
parada.

> **Não é preciso credencial de nenhum remote Conan privado** — `./scripts/deps.sh`
> (`INSTALL.md` §4) builda tudo do fonte, o mesmo caminho que o CI usa.
> **`CLAUDE.md` não é material de leitura para este tour** — é um diário de arquitetura para
> sessões de IA, cronológico, não pedagógico; consulte só pontualmente se outro documento apontar
> pra lá.
> **Tacview é aplicativo de terceiro**, edição Advanced (paga) — sem ela não há telemetria ao vivo.

## Trilha principal

### 1. Subir o ambiente

**Objetivo:** `./dist/bin/app` funcionando.
**Onde:** `README.md` ("Pré-requisitos"/"Build"); `INSTALL.md`. 
**Sucesso:** `make clean configure sdk models build install` sem erro; `make run-app` abre a tela
de seleção de cenário.

### 2. Rodar um cenário e visualizar no Tacview

**Objetivo:** ver uma aeronave se movendo e trocando de comportamento, ao vivo, no Tacview.
**Onde:** seção "Rodar" do `README.md`; `src/poc/dis/README.md` (portas, e por que precisa rodar
`bandit` + `flight` juntos — sem o intruso não há o que evadir).
**Sucesso:** Tacview conectado, telemetria chegando, uma manobra de troca de comportamento visível
(evasão/alerta/apoio).

### 3. Explorar o `app` (o painel de controle / TUI)

**Objetivo:** navegar pelas abas (Players, Mapa, Memória, Tempo Não-Crítico, Log, Componentes, EDL)
com uma simulação rodando.
**Onde:** `app/README.md`.
**Sucesso:** identificar em que thread cada player decide, pausar/acelerar o tempo, abrir o card de
detalhe de uma entidade.

### 4. Abrir o manual interativo

**Objetivo:** usar as cinco visões sobre o framework — ciclo de execução, cadeia de decisão,
catálogo de classes, entre outras.
**Onde:** `make open-docs`; seção "Leia mais" do `README.md`.
**Sucesso:** localizar, na aba de comportamento, a cadeia real de produção `FlightAgentTC →
Agent::controller → BtBehavior → flight_tree.xml → FlightAction`.

## Trilha de aprofundamento (se sobrar tempo)

### 5. Editor gráfico de cenário (`edl-builder`)

**Objetivo:** montar um `.edl` arrastando classes de uma paleta, exportar, validar.
**Onde:** `make open-edl`; `src/ui/README.md`.
**Sucesso:** exportar um `.edl` que passa sem erro em `./dist/bin/edlcheck <arquivo>`.

### 6. Editar/monitorar uma árvore de comportamento com o Groot

**Objetivo:** abrir `models/players/air/A-4/configs/flight_tree.xml` no Groot (terceiro).
**Onde:** `CONTRIBUTING.md`, seção 5 ("Escreva a lógica").
**Sucesso:** a árvore abre sem erro de "modelo não registrado" e dá para editar/salvar.

### 7. Rodar a poc com decisão em Python

**Objetivo:** rodar `python-flight` e editar um script de decisão vivo, sem recompilar.
**Onde:** `src/poc/python-flight/README.md`.
**Sucesso:** mudar um `.py` de política altera o comportamento observável na próxima execução.

### 8. `src/rl` — o ambiente de RL

**Objetivo:** entender o wrapper Gymnasium e rodar o smoke test contra a simulação de verdade.
**Onde:** `src/rl/README.md`.
**Sucesso:** o teste de fumaça do ambiente passa.

### 9. `src/poc/rl-training` — treinar uma política

**Objetivo:** treinar via PPO (Stable-Baselines3) e exportar para `.onnx`.
**Onde:** `src/poc/rl-training/README.md`; `src/poc/onnx-policy/README.md` para rodar o `.onnx`.
**Sucesso:** um `.onnx` novo carrega sem erro na poc `onnx-policy` (não precisa voar igual ao de
referência). Pode facilmente passar de 45 min — deixe rodando em background se precisar.

### 10. Criar um modelo novo como plugin

**Objetivo:** gerar o scaffold e ver seu próprio `.so` carregado por um cenário.
**Onde:** `CONTRIBUTING.md`; confira `models/REGISTRO.md` antes de começar.
**Sucesso:** `make new-model NAME=<nome> CATEGORY=players/air` gera o projeto, `make models`
compila, e um cenário próprio carrega o `.so`.