# O que um modelo tem de fazer

Este documento existe porque o `PluginAbi.hpp` **não é o contrato inteiro**. Ele é o contrato de
*empacotamento*: ponto de entrada, inteiros de ABI, fábrica por nome, `MetaObject`. O que a
aplicação de fato exige de um modelo estava espalhado entre o cenário, as classes base do MIXR e
uma disciplina não escrita — e foi escrever o `stub-model` que obrigou a descobrir a lista.

O `stub-model` é o teste desta página: um modelo de ~270 linhas, sem árvore de comportamento e sem
nenhuma regra de `domain/`, que faz o cenário de **produção** rodar. Se algum item abaixo estiver
errado ou faltando, `meson test --suite plugin` fica vermelho em `plugin-modelo-estranho`.

## 1. Empacotamento

Exportar `mixr_plugin_v1` via `MIXR_PLUGIN_DEFINE` (nunca à mão — num alvo com
`-fvisibility=hidden` a assinatura escrita à mão vira símbolo invisível ao `dlsym`). Linkar com
`-Wl,--no-undefined` e, se houver biblioteca estática no link, `-Wl,--exclude-libs,ALL`.

## 2. Os nomes de fábrica, e eles vêm do CENÁRIO

O `.epp` do host nomeia as classes e **os slots delas, com tipo e unidade**. `provides:` é
igualdade exata de conjunto, e um nome desconhecido no cenário **aborta o processo**. A lista de
hoje:

| nome de fábrica | classe base obrigatória | por quê |
|---|---|---|
| `FlightState` | `base::ubf::AbstractState` | slot `state:` do agente |
| `BtBehavior` | `base::ubf::AbstractBehavior` | dentro do `( UbfArbiter )` |
| `AltitudeSafetyBehavior` | `base::ubf::AbstractBehavior` | idem |
| `FlightAction` | `base::ubf::AbstractAction` | devolvida por `genAction()` |
| `AlertDatalink` | `models::Datalink` | `Player::processComponents` casa por `findByType(typeid(models::Datalink))` |
| `TacticalAlert` | `base::Object` | carga útil |
| `FlightAgentTC` | `base::ubf::AgentTC` | só na multi-thread |

Os slots que o cenário usa estão em `models/stub-model/src/stub.cpp` — as tabelas ali são a lista
mínima. Um slot que o modelo não conheça faz o parser somar erro e o host sair.

## 3. A obrigação que falha em SILÊNCIO

**O modelo tem de escrever no `xboard`.** Nada no descritor, no `.pc` ou no header diz isso, e é a
única obrigação cuja ausência não gera erro nenhum:

```cpp
xboard::setBehaviorLabel(playerId, "PATROL");   // no ponto da ATUACAO
xboard::bumpDecisionCount(playerId);
xboard::setAlert(playerId, valido, remetente, contato);   // se houver datalink
xboard::setDatalinkCounters(playerId, enviados, recebidos);
xboard::setThreadTag(playerId, tag);                      // se decidir no pool T/C
xboard::setRadarScan(playerId, achou, az, el, alcance, feixeH, feixeV);  // na percepcao
```

A varredura de radar é o caso mais claro de "isto é do modelo, não do host": quem sabe para onde a
antena está apontando é quem percebe. O host só relaia ao Tacview o que o quadro disser. Um modelo
que nunca publique deixa `radarValid` em `false` e simplesmente não desenha varredura — degradação
silenciosa **deliberada**, porque um modelo sem radar é legítimo.

O `stub-model` lê o `Gimbal` direto pela API do MIXR, em oito linhas, sem usar o `RadarScan` do
`flight-model`. É a prova de que a obrigação é **publicar**, e não usar um helper nosso.

Sem isso o host imprime `bt=--`, `dec=0`, `alert=none`, `sent=0`, `recv=0` — **e todos os outros
testes ficam verdes**: o descritor é coerente, os 6 nomes constroem, o binário não tem símbolo do
modelo, o SDK bastou para linkar. Só `plugin-modelo-estranho` pega.

## 4. O `.so` não é a entrega completa

O slot `treeFile:` do `( BtBehavior )` nomeia um arquivo que é **do modelo**, não do cenário. O
`flight-model` instala o dele em `dist/share/mixr-plugins/flight-model/`. Um modelo que precise de
dados próprios instala junto e o cenário aponta para lá.

## 5. O que o contrato NÃO impõe

Nem pilha interna, nem árvore de comportamento, nem `domain/`. O `stub-model` não linka a
BehaviorTree.CPP e decide com uma linha. Isso também é parte do que se verifica.

## Limites conhecidos

- A lista da seção 2 é derivada do cenário **de hoje**. Um cenário novo que nomeie outra classe
  quebra o stub — e isso é bom: o teste vira o lembrete de atualizar esta página.
- Nada aqui é verificado pelo compilador. A verificação é o `stub-model` rodando; não há como o
  contrato ser conferido em tempo de compilação sem uma interface abstrata, que traria de volta o
  acoplamento de vtable que o `PluginDescV1` existe para evitar (ver a seção *Limites* de
  `shared/xplugin/README.md`).
