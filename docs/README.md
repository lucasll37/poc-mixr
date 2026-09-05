# `docs/` — visualizador do ciclo de fases e dos eventos do framework MIXR

`index.html` é uma página estática, sem dependências, sem build, com duas visões animadas
sobre a árvore de CLASSES que o framework MIXR fornece — não um cenário específico:

1. **Ciclo de fases** — o ciclo de 6 fases do frame (`Structural`, `dynamics()`,
   `transmit()`/`receive()`, `process()`, `Agent::controller()` em segundo plano,
   `updateData()` em segundo plano), avançando sozinho, destacando quem participa de cada
   fase e com que método.
2. **Eventos (emissão/tratamento)** — o mecanismo genérico de eventos do framework
   (`Component::event()` + as macros `BEGIN_EVENT_HANDLER`/`ON_EVENT`/`ON_EVENT_OBJ`),
   mostrando quem EMITE e quem TRATA cada token nativo (`RESET_EVENT`, `SHUTDOWN_EVENT`,
   `KILL_EVENT`, `CRASH_EVENT`, `DATALINK_MESSAGE`), mais um exemplo de token definido pela
   própria aplicação (`EID_ALERT`) provando que o mecanismo é genérico o bastante para
   qualquer app estender.

A ideia não é nova neste repositório: o ciclo de fases é a mesma visão já comprovada na aba
"Componentes" (F6) do `./app` (o dashboard FTXUI) — `app/src/app/ComponentTreeQuery.cpp`,
`ComponentFlowState.cpp`, `FrameCallChain.cpp`, `ComponentTreePanel.cpp`. Esta página adapta
esse modelo para o navegador e acrescenta a camada de eventos, que a aba F6 não tem.

**Curada, não instrumentada.** Sem processo MIXR rodando por trás, a árvore, as fases e os
eventos foram lidos direto do código-fonte do framework (`contexts/src/mixr/`, fork v170600)
— cada afirmação da página (nome de classe, método, `arquivo:linha`) foi verificada contra
esse fonte, não inventada. Isso está avisado na própria página (banner amarelo no rodapé) e
não deve ser removido em incrementos futuros.

## Como abrir

Direto no navegador, sem servidor nenhum (zero requisições de rede):

```
xdg-open docs/index.html     # ou file://.../docs/index.html
# make open-docs também funciona (ver Makefile)
```

Abrir direto na aba de eventos: `docs/index.html#events` (link compartilhável).

Ou servido (para simular hospedagem futura, ex. GitHub Pages):

```
python3 -m http.server --directory . 8000
# abrir http://localhost:8000/docs/
```

## O que a v2 mostra

- **A árvore é de CLASSES do framework, não de instâncias de um cenário**: `Station` →
  `Simulation` → `Player` → os subsistemas genéricos (`DynamicsModel`, `Pilot`, `Datalink`,
  `Navigation`, `Radio`, `Gimbal`→`Antenna`, `RfSensor`, `IrSensor`,
  `OnboardComputer`→`TrackManager`, `StoresMgr`, `CollisionDetect`, e as duas variantes de
  `ubf::Agent`), mais `AbstractDataRecorder`→`DataRecorder`→`OutputHandler`,
  `AbstractNetIO`→`NetIO` e `AbstractIoHandler`→`IoHandler`. Nenhum nome de player, slot de
  cenário ou classe própria desta aplicação aparece — só o que o MIXR em si fornece.
- **Classes abstratas** (`AbstractDataRecorder`, `AbstractNetIO`, `AbstractIoHandler`,
  `AbstractBehavior`) ganham um contorno tracejado e o prefixo «abstract» — nunca são
  usadas diretamente, sempre por uma subclasse concreta.
- **Duas relações distintas na árvore**: composição normal (linha sólida, pai→filho no
  EDL) e referência POR NOME (linha pontilhada curva, ex. `RfSensor` → `Antenna`/
  `TrackManager` pelos slots `antennaName`/`trackManagerName`) — a segunda não é
  composição, é resolvida em tempo de execução por `findByName()`.
- **Aba Eventos**: emissor em âmbar, tratador em azul, auto-dirigido (mesmo nó emite e
  trata) em roxo, cascata (todo mundo trata) em rosa com as arestas da árvore inteira
  "fluindo". Setas cruzam a árvore em QUALQUER direção — inclusive de filho para pai
  (`CollisionDetect`→`Player`, `Datalink`→`Player`), o oposto do sentido da recursão de
  fase, que só desce.
- Os mesmos controles (tocar/pausar/velocidade/passo/reiniciar) funcionam nos dois modos —
  é a mesma sequência animada, só troca o que ela percorre.

## O que fica para os próximos incrementos (cortado de propósito na v2)

- Pan/zoom e colapsar/expandir ramos (a árvore inteira é sempre desenhada).
- Mais tokens de evento (hoje só os 5 nativos mais usados + 1 exemplo de app; faltam
  `WPN_REL_EVENT` e a família de tokens de HOTAS, documentados mas não emitidos por nada
  hoje neste repositório).
- Diferenciar visualmente "emitido por chamada direta" (`RESET_EVENT`) de "emitido por
  reemissão recursiva" (`SHUTDOWN_EVENT`) — hoje as duas cascatas usam o mesmo estilo
  visual; a distinção só está no texto do painel.
- Uma "onda" de sub-passo percorrendo o caminho aceso entre um passo e o próximo (o
  equivalente ao `flowSubStep()` da aba F6 do `./app`) — hoje a troca é discreta.
- Carregar árvore/eventos de um JSON externo em vez de um literal inline, se um dia isto
  precisar cobrir mais de uma versão do framework.
