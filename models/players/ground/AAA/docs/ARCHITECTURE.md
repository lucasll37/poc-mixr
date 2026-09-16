# Arquitetura de `AAA`

## O que este modelo é, e o que ele NÃO é

Uma antiaérea (bateria de superfície-ar) estacionária que detecta um alvo hostil pelo próprio
radar de aquisição, e dispara `( GuidedMissile )` (`models/players/weapon/missile`, sem nenhuma
modificação) quando o alvo entra no "domo" de alcance. **Não é um sistema de armas de produção** —
é um exercício para verificar como esse mecanismo é modelado de forma idiomática no MIXR (ver
`TODO.md`, raiz).

## A decisão de arquitetura: BT/UBF completo, mesmo para uma regra única

A decisão inteira desta antiaérea cabe numa frase: *"se há um alvo hostil dentro do domo e há
munição disponível, dispara."* Isso é fundamentalmente diferente do A-4 (`models/players/air/A-4`),
cuja árvore arbitra QUATRO prioridades concorrentes (combustível baixo, contato detectado, alerta
recebido, patrulha) — aqui não há prioridades concorrentes nenhuma, só uma condição.

Duas opções foram avaliadas e apresentadas ao usuário (`AskUserQuestion`) antes de implementar:

1. **Uma classe `xnative` mínima**, sem `bt/`/`ubf/` — a regra decidida direto em `updateTC()`,
   mesmo padrão que `models/players/weapon/missile` já usa para a guiagem (que também não tem árvore,
   porque é controle contínuo, não decisão discreta). Teria bastado, e seria ~3× menos código.
2. **A pilha completa `domain/` → `bt/` → `ubf/` → `xnative/`**, com uma árvore de comportamento
   de verdade — a opção escolhida, deliberadamente, para espelhar a arquitetura do A-4 mesmo numa
   regra única. O ganho não é técnico (a árvore de dois ramos não faz nada que um `if` não
   faria) — é pedagógico: verificar como o MESMO padrão de camadas (percepção → árvore → ação,
   `bt::DecisionContext` mantendo os nós livres de MIXR, `xboard` como a única obrigação que falha
   em silêncio) se aplica a um lançador terrestre, não só a uma aeronave.

A árvore em si (`configs/aaa_tree.xml`) é só dois ramos:

```
Fallback (root)
  Sequence: TargetInDome -> FireMissile     (rotulo FIRE)
  Watch                                     (sempre SUCCESS, rotulo WATCHING)
```

O segundo ramo (`Watch`) existe só para a árvore SEMPRE produzir uma decisão observável
(`bt=WATCHING` no xboard/dump) mesmo sem alvo — sem um último ramo incondicional, um `Fallback`
devolveria `FAILURE` quando nada casasse, e a antiaérea ficaria sem decisão nenhuma no frame (a
mesma obrigação documentada em `models/template/docs/CONTRATO.md`, seção 3).

## Por que NÃO há nenhuma subclasse de `AgentTC` aqui

O A-4 precisou escrever `xnative::FlightAgentTC` (`models/players/air/A-4/include/xnative/
FlightAgentTC.hpp`) porque a variante de TEMPO CRÍTICO do agente UBF nativo
(`mixr::base::ubf::AgentTC`, decide na fase 3 do frame, factory name `"UbfAgentTC"`) **não** é
registrada por nenhuma factory nativa do MIXR — confirmado lendo
`contexts/src/mixr/src/base/ubf/Agent.cpp`: só `IMPLEMENT_SUBCLASS(Agent, "UbfAgent")` (a variante
de FUNDO, `updateData()`) aparece encadeada em `base/factory.cpp`; `AgentTC` não.

Uma antiaérea estacionária não precisa decidir à taxa de tempo crítico (50 Hz) — um alvo a
~100 m/s entra no domo e há de sobra a taxa de fundo (10 Hz, ~100 ms de latência) para "alcance
dentro do domo → dispara". Por isso o `.edl` do cenário declarava `( UbfAgent state: (AaaState)
behavior: (AaaBehavior) )` **direto, nativo, sem nenhum C++ novo** (até a correção descrita logo
abaixo, "`AaaAgent` — o ciclo de referência com o próprio player"):

```cpp
// contexts/src/mixr/src/base/ubf/Agent.cpp
void Agent::initActor() {
   if (getActor() == nullptr && container() != nullptr) setActor(container());
}
```

`Agent::initActor()` já resolve o ator como o próprio `container()` do componente `agent:` —
como este é declarado DENTRO de `components:` da `AaaSite`, o ator já é a própria antiaérea, sem
precisar subir a cadeia de containers (diferente do `FlightAgentTC` do A-4, que também não
precisou sobrescrever `initActor()` pelo mesmo motivo — ver o comentário daquela classe).

**Isto continua verdadeiro** — nenhuma taxa de tempo crítico, nenhum `AgentTC`. O que mudou foi
outra coisa, ortogonal a isto: ver a seção seguinte.

## `AaaAgent` — o ciclo de referência com o próprio player

O mesmo `Agent::initActor()` citado acima — `setActor(container())` — é exatamente o que fecha um
ciclo de referência clássico: `AaaSite` possui o agente via `components:` (uma referência forte,
como qualquer `PairStream` de componentes), e o agente possui uma referência de volta a `AaaSite`
via `myActor` (`base::safe_ptr`, também ref-owning). Nenhum `unref()` externo desfaz um ciclo
assim sozinho — `Agent::deleteData()` (que zera `myActor`) só roda quando o próprio `Agent` já
está sendo destruído, o que nunca acontece enquanto o ciclo segura os dois lados acima de zero.

Isto não é hipotético: é o MESMO bug já encontrado e corrigido em `models/players/air/A-4`
(replicado em `C-130`/`paratrooper`/`Navstar-3`) — medido por `LeakSanitizer` vazando a árvore de
objetos inteira de um player no modo `-deterministic`, porque o processo sai antes que qualquer
coisa force o ciclo a se desfazer. A diferença aqui é que, nos outros quatro modelos, a classe de
agente já existia por outro motivo (`AgentTC`, decisão em tempo crítico) e só precisou ganhar um
`shutdownNotification()` a mais; aqui não havia NENHUMA classe de agente própria — só o
`( UbfAgent )` nativo, direto — então a correção precisou nascer uma classe nova,
`xnative::AaaAgent` (`include/xnative/AaaAgent.hpp`), só para isso:

```cpp
bool AaaAgent::shutdownNotification()
{
   setActor(nullptr);
   return BaseClass::shutdownNotification();
}
```

`AaaAgent` **não muda nenhum comportamento de decisão** — `controller()`/`initActor()`/
`updateData()` continuam exatamente os de `base::ubf::Agent`, herdados sem override. O `.edl` do
cenário passou a declarar `( AaaAgent state: (AaaState) behavior: (AaaBehavior) )` no lugar de
`( UbfAgent ...)`; `provides: { ... AaaAgent }` precisou da entrada nova pelo mesmo motivo de
sempre (`provides:` é igualdade exata de conjunto contra o que `libAAA.so` exporta).

**Não verificado por `LeakSanitizer` de verdade** (ao contrário do A-4/core, os únicos alvos que
consomem `asan_cpp_args`/`asan_link_args` hoje) — a prova aqui é por leitura do fonte (o mesmo
`Agent::initActor()`/`safe_ptr` citados acima) mais build + suíte de testes + execução completa
do cenário `sandbox/AAA-A4-6DOF` em `-deterministic`, sem crash. Estender ASan a este projeto
ficaria para quem quiser essa prova instrumentada.

## Armadilha: `SamVehicle`/`Sam` não são o mesmo que `Missile`

`xnative::AaaSite` é subclasse de `mixr::models::SamVehicle` **só** para herdar os slots nativos
`minLaunchRange`/`maxLaunchRange` (o "domo" de alcance, já nativo do MIXR — não foi preciso
reinventar isso). Mas `SamVehicle::updateData()` conta munição contando quantos itens do
`stores:` fazem `dynamic_cast<const Sam*>` com sucesso:

```cpp
// contexts/src/mixr/src/models/player/ground/SamVehicle.cpp
numMsl = 0;
// ... para cada store: if (dynamic_cast<const Sam*>(pair->object()) != nullptr) numMsl++;
```

`mixr::models::Sam` é uma subclasse quase vazia de `Missile` (só sobrescreve `getDescription()`/
`getCategory()`, sem guiagem própria). `xmissile::GuidedMissile` (`models/players/weapon/missile`) também
é subclasse direta de `Missile` — **as duas são IRMÃS**, não uma filha da outra. Um `( GuidedMissile
)` no `stores:` nunca casa o `dynamic_cast<const Sam*>`, então `SamVehicle::getNumberOfMissiles()`/
`isLauncherReady()` ficam **sempre 0/falso**, em silêncio — nenhum erro, nenhum aviso, só a
antiaérea nunca disparando e nada explicando o porquê.

`ubf::AaaState` usa `StoresMgr::available() > 0` diretamente (a mesma checagem que o lançador do
A-4 já usa) — **nunca** `SamVehicle::isLauncherReady()`/`getNumberOfMissiles()`. Isso foi
descoberto por auditoria (lendo o fonte do `SamVehicle` antes de escrever qualquer código), não
depois de medir um disparo que nunca acontecia.

## Reuso do `GuidedMissile`, confirmado sem nenhuma modificação

A sequência de disparo (`ubf::AaaAction::execute()`) é **idêntica** à já usada pelo A-4
(`models/players/air/A-4/src/ubf/FlightAction.cpp:299-328`):

```cpp
auto* const storesMgr = site->getStoresManagement();
auto* const flyout = storesMgr->releaseOneMissile();   // pre-ref'd, dynamic_cast<Missile*> interno
flyout->setTargetPlayer(target, /*posTrkEnb=*/true);
flyout->unref();
```

`xmissile::GuidedMissile` nunca inspeciona quem o lançou — só chama `getTargetPlayer()`. Confirmado
nesta mesma sessão de trabalho: a MESMA classe de míssil, sem recompilar nem tocar uma linha, serve
tanto ao lançador aéreo (A-4, `Aircraft`) quanto ao terrestre (esta antiaérea, `SamVehicle`/
`GroundVehicle`).

## Estacionária por configuração de `.edl`, não por código

Sem `dynamicsModel:` e com `initVelocity: 0.0`, a `AaaSite` fica parada na posição inicial —
`Player::dynamics()` só invoca um modelo dinâmico `if (getDynamicsModel() != nullptr)`; a
integração de posição roda de qualquer forma, mas com velocidade zero nada se move. Mesmo idioma já
usado pelo modelo paratrooper antes do lançamento (o `Paratrooper` parado, ver
`models/players/effect/paratrooper`).

## Por que `domain::` mora dentro de `mixr::models::xAAA`, e não solto

Mesmo raciocínio documentado no `docs/ARCHITECTURE.md`/`docs/CONTRATO.md` do
`models/template/` que originou este scaffold: um cenário poderia, em tese, carregar mais de um
plugin no mesmo processo (este `AAA` ao lado de `missile`/`A-4`, o que de fato acontece no
cenário `sandbox/AAA-A4-6DOF`), e dois tipos com o MESMO nome qualificado (`domain::Foo`) em dois
`.so`s distintos colidiriam sob `RTLD_LOCAL` (a comparação de `type_info` deste toolchain degrada
para `strcmp` do nome *mangled*). `models/players/air/A-4` (`domain::` solto no escopo global) é a
exceção histórica documentada, não o exemplo a copiar.

## Ler também

- [`../README.md`](../README.md) — como compilar, testar e instalar este diretório sozinho.
- [`../../../missile/docs/ARCHITECTURE.md`](../../../missile/docs/ARCHITECTURE.md) — por que o míssil
  em si NÃO tem árvore de comportamento (guiagem é controle contínuo).
- [`models/template/docs/CONTRATO.md`](../../../template/docs/CONTRATO.md) — a lista completa e
  autoritativa do que qualquer modelo precisa fazer.
- `CLAUDE.md`, raiz, seção "O modelo MIXR em uma tela" — o ciclo de fases e o padrão geral.
