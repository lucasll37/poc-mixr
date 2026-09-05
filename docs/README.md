# `docs/` — visualizador do ciclo de simulação MIXR

`index.html` é uma página estática, sem dependências, sem build, que mostra — de forma
animada e recursiva — a árvore de componentes de uma simulação MIXR e o ciclo de fases
que o framework percorre em cada frame, indicando qual método/função roda em cada
componente a cada fase.

A ideia não é nova neste repositório: é a mesma que já existe, comprovada, na aba
"Componentes" (F6) do `./app` (o dashboard FTXUI) — `app/src/app/ComponentTreeQuery.cpp`,
`ComponentFlowState.cpp`, `FrameCallChain.cpp` e `ComponentTreePanel.cpp`. Esta página é
uma adaptação **estática e mínima** desse mesmo modelo conceitual para o navegador: sem
processo MIXR rodando por trás, a árvore e a fase de cada nó foram **curadas à mão**,
lendo o cenário real (`src/poc/dis/multi-thread/configs/scenario.edl.in`) e o fonte do
framework (`contexts/src/mixr/`) — não medidas ao vivo. Isso está avisado na própria
página (banner amarelo no rodapé) e não deve ser removido em incrementos futuros.

## Como abrir

Direto no navegador, sem servidor nenhum (zero requisições de rede):

```
xdg-open docs/index.html     # ou file://.../docs/index.html
```

Ou servido (para simular hospedagem futura, ex. GitHub Pages):

```
python3 -m http.server --directory . 8000
# abrir http://localhost:8000/docs/
```

## O que a v1 mostra

- As 6 fases do ciclo do frame (`Structural`, `DynamicsPhase0`, `SensorPhase1And2`,
  `DecisionPhase3`, `DecisionBackground`, `Background`), avançando sozinhas num relógio
  próprio (não sincronizado a nenhum processo real), com controles de tocar/pausar,
  velocidade (1x/2x/4x) e passo manual.
- A árvore de componentes da poc `multi-thread`, com `falcon1` expandido por completo e
  `falcon2..4` resumidos num único nó (mesma estrutura, omitidos por simplicidade).
- Por fase: os nós participantes acendem, o caminho raiz→nó fica colorido, e aparece o
  nome da chamada (`dynamics(dt4)`, `process(dt4)`, `controller(dt)`...) sob cada nó ativo.
- Um painel com a cadeia de chamadas real do framework para a fase corrente
  (função + `arquivo:linha`, verificados contra o fonte do MIXR v1.0.5).
- Um painel de detalhe ao clicar em qualquer componente.
- A fase `DecisionBackground` fica deliberadamente sem participante nesta árvore — a
  poc `multi-thread` decide na fase 3, não em segundo plano. Isso é avisado na tela, não
  é um bug.

## O que fica para os próximos incrementos (cortado de propósito na v1)

- Pan/zoom na árvore (hoje ela só escala pelo `viewBox`, sem arrastar).
- Colapsar/expandir ramos (hoje a árvore inteira é sempre desenhada).
- Um seletor de cenário `single-thread` vs `multi-thread`, para mostrar de verdade a fase
  `DecisionBackground` com um `(SimAgent)` participando (hoje só existe a nota explicando
  a ausência).
- As 4 aeronaves (`falcon1..4`) animando levemente fora de fase entre si, para sugerir
  visualmente o pool de threads de tempo crítico decidindo em paralelo.
- Uma "onda" de sub-passo percorrendo o caminho aceso entre uma fase e a próxima (o
  equivalente ao `flowSubStep()` da aba F6 do `./app`) — hoje a troca de fase é discreta.
- Carregar a árvore/cadeia de chamadas de um arquivo JSON externo em vez de um literal
  inline, se um dia isto precisar descrever mais de um cenário.
