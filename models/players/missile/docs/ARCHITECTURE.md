# Arquitetura deste modelo

## Duas camadas, não quatro

`models/players/template/` (a origem deste diretório) demonstra `domain/` → `bt/` → `ubf/` →
`xnative/`, a forma que um modelo com **decisão discreta** (uma árvore de comportamento) usa.
Este modelo não decide nada discretamente — guiar um míssil é um problema de **controle
contínuo** (um comando de rumo/arfagem recalculado a cada frame a partir da geometria
relativa ao alvo), e é exatamente por isso que o próprio `mixr::models::Missile` nativo não
usa `BehaviorTree.CPP`/UBF para guiagem — ele reserva dois métodos protegidos,
`weaponGuidance(dt)`/`weaponDynamics(dt)`, chamados diretamente por
`AbstractWeapon::dynamics()` sempre que `getDynamicsModel() == nullptr` (confirmado lendo
`contexts/src/mixr/src/models/player/weapon/AbstractWeapon.cpp:289`, não suposto).

Ficam só:

```
domain/    -- a lei de guiagem e a espoleta, PURAS. Sem MIXR, sem ponteiro
              de framework nenhum. Testável com um executable() que nem
              linka o MIXR (ver tests/meson.build) -- a mesma camada
              domain/ que models/players/A-4 usa para ThreatPolicy/PatrolPlan,
              aqui sem bt/ubf por cima porque não há decisão discreta.

xnative/   -- a classe MIXR (GuidedMissile, subclasse de
              mixr::models::Missile) + a factory que o boundary do plugin
              chama (src/plugin.cpp). Onde domain/ encontra o mundo do
              MIXR: lê Player*/Player::getVelocity() do alvo, escreve de
              volta via setEulerAngles()/setVelocity().
```

## Por que cinemático, e não `JSBSimModel` — a decisão mais consequente daqui

Já existiu, neste mesmo diretório, uma versão anterior com um `JSBSimModel` próprio (uma
aeronave `"aim1"` vendorizada, de baixa inércia) e um `dynamics()` sobrescrito para rodar
esse modelo de voo completo. Essa abordagem funciona — está comprovada em commits antigos
deste repositório (recuperável via `git show`) — mas tem dois custos que esta versão evita
deliberadamente:

1. **Precisa vendorizar uma aeronave JSBSim inteira** — arquivos de configuração de massa,
   aerodinâmica, motor — calibrada especificamente para não divergir numericamente num corpo
   pequeno e de giro rápido (a versão antiga documentava ter reduzido `emptywt` mas **não**
   os momentos de inércia, porque zerar os dois juntos divergia).
2. **É repropositar um modelo de voo de AERONAVE para representar um MÍSSIL** — o oposto do
   que o próprio framework oferece: `mixr::models::Missile` já reserva dois métodos
   protegidos, `weaponGuidance()`/`weaponDynamics()`, especificamente para um "míssil
   cinemático simples" sem `dynamicsModel` nenhum (ver o comentário da classe em
   `contexts/src/mixr/include/mixr/models/player/weapon/Missile.hpp`). Usar JSBSim para o
   míssil é possível, mas usar os *hooks* dedicados é o caminho que o framework desenhou
   para isto — o mais idiomático das duas opções, e o que este exercício escolheu.

A troca é objetiva: sem aeronave nova pra calibrar, sem risco de divergência numérica de
integrador de voo, guiagem/dinâmica inteiramente pura e testável em `tests/domain/` sem
levantar `Station` nenhuma.

## A lei de guiagem: navegação proporcional de verdade, não perseguição pura

`mixr::models::Missile::weaponGuidance()` nativo (o que `( AamMissile )`/`( Sam )`/etc. usam
quando ninguém sobrescreve nada) já implementa uma guiagem por **ponto de interceptação**
(extrapola a posição do alvo pelo tempo estimado de fechamento e mira nesse ponto futuro) —
funciona, mas não é navegação proporcional: não reage à **taxa** de variação da linha de
visada, só a um snapshot geométrico do instante atual.

`domain::proportionalNavigation()` (`include/domain/Guidance.hpp`) é diferente: soma
**perseguição pura** (mira direto no ângulo atual da linha de visada) com uma correção
proporcional à **taxa** dessa linha de visada (`cmdHeading = losAz + N' × losAzRate`, o termo
clássico `N' × Vc × λ̇` da navegação proporcional real, aqui simplificado para heading/pitch
comandados em vez de aceleração cartesiana — o que `weaponDynamics()`, com seu limitador de
taxa de giro, já sabe integrar diretamente). Contra um alvo em curva, perseguição pura sozinha
fica cronicamente atrás da trajetória; corrigir pela taxa da LOS adianta a rota antes do erro
angular crescer. Medido rodando (não só em teste unitário): um míssil a 280 m/s convergindo
contra um alvo cruzando a 60 m/s a 8,5 km de distância fecha para menos de 200 m de alcance
mínimo — ver `tests/domain/test_Guidance.cpp::ConvergeContraAlvoEmCruzamento`, com os números
exatos no comentário do teste.

## A espoleta: mesma semântica do `Missile` nativo, reescrita como função pura

`Missile::weaponGuidance()` nativo detona ao detectar a transição "alcance diminuindo →
alcance aumentando" (o ponto de menor aproximação), comparando a distância nesse instante
contra `maxBurstRng` — acerta (`DETONATE_ENTITY_IMPACT`) ou autodestrói por ter passado do
alvo sem acertar (`DETONATE_DETONATION`), sempre. `domain::proximityFuze()` replica
exatamente essa semântica como função pura: recebe o estado do frame anterior
(`FuzeState{hasSample, wasApproaching}`) e o devolve atualizado — sem guardar nada por conta
própria —, porque detectar uma **transição** exige comparar dois instantes, não um só.
`xnative::GuidedMissile` guarda esse estado como membro privado entre frames.

## O que este modelo NÃO tem, de propósito

- **Nenhum slot novo no `.edl`** — `EMPTY_SLOTTABLE`. Todos os parâmetros que fariam sentido
  configurar (velocidade de cruzeiro, G máximo, alcance letal/de explosão, tempo de voo) já
  são slots herdados de `mixr::models::Missile`/`AbstractWeapon` (`maxSpeed`, `maxg`,
  `lethalRange`, `maxBurstRng`, `maxTOF`, ...) — o construtor deste modelo só ajusta os
  *defaults* deles para valores plausíveis de um míssil ar-ar de curto alcance.
- **Nenhuma integração de posição própria** — `weaponDynamics()` só escreve
  velocidade/atitude (`setVelocity()`/`setEulerAngles()`); a posição é integrada
  genericamente por `Player::dynamics()` → `positionUpdate(dt)`, chamado logo depois por
  `AbstractWeapon::dynamics()` (`BaseClass::dynamics(dt)`, confirmado lendo o fonte). Integrar
  posição nos dois lugares duplicaria a conta.
- **Remoção pós-detonação** — a única coisa que este modelo acrescenta além dos dois *hooks*
  de guiagem: nada nativo transiciona um `Missile` de `DETONATED` para `DELETE_REQUEST` (nem
  `AbstractWeapon::updateTOF()` nem `weaponGuidance()` fazem isso — ambos só chamam
  `setMode(DETONATED)`), então sem esse passo o míssil detonado ficaria pra sempre na lista de
  players/no Tacview. `GuidedMissile::updateTC()` acumula um timer curto (2s, só na fase 3,
  mesmo gating de fase que `AbstractWeapon` já usa para o TOF) e só então pede a remoção.

## Ler também

- [`../README.md`](../README.md) — como compilar, testar e instalar este diretório sozinho
- [`../../../CLAUDE.md`](../../../CLAUDE.md), seção "O MODELO é um plugin, construído numa
  etapa PRÉVIA" — visão geral de `models/`, o contrato de plugin, e o build orquestrado
- [`../../template/docs/CONTRATO.md`](../../template/docs/CONTRATO.md) — a lista completa e
  autoritativa do que qualquer modelo precisa fazer para o host carregá-lo
