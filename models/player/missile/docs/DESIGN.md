# `missile` — notas de design

Segundo modelo de exemplo (ver [CLAUDE.md](../../../CLAUDE.md), seção "Demo: míssil guiado", para
o contexto completo — lançamento/detonação/destruição, e o "porquê" de ser um plugin à parte em
vez de entrar em `models/A4/`). Este arquivo é só a parte de **guiagem**, a peça própria deste
modelo.

## A lei de guiagem — `domain::pursuit()`

Perseguição pura (*pure pursuit*), não navegação proporcional de verdade — decisão deliberada,
documentada em [`src/domain/Guidance.hpp`](../src/domain/Guidance.hpp): navegação proporcional
exigiria estimar a taxa de variação da linha de visada filtrada contra ruído; para um alvo único,
não manobrando, dentro do alcance curto desta demo, perseguição pura converge e é trivial de
explicar.

- **Entra** em NED, metros e graus (mesma convenção de `domain::WorldView` do `flight`).
- **Sai** comando normalizado (`-1..1`), pronto para `Player::setControlStickRollInput()`/
  `PitchInput()` — a mesma faixa que o joystick físico já usa (`shared/xjoystick`).
- **Proporcional-derivativo, não só proporcional.** O termo de taxa (`rollRateGainDps`/
  `pitchRateGainDps`) é o amortecimento que falta a um controlador só-proporcional — sem ele, a
  `tests/domain/test_Guidance.cpp` (`TaxaPropriaAmortece`) trava exatamente essa propriedade:
  taxa própria já no sentido do comando **reduz** a magnitude do comando final.

## Armadilha confirmada rodando — não redescobrir

Reduzir os momentos de inércia do `aim1.xml` na mesma proporção do peso (para "mais carga G")
deixa o modo de arfagem/rolagem pouco amortecido demais para o passo fixo de 0,02 s do JSBSim —
medido divergindo (mais de 100° de banco/arfagem, velocidade escalando para milhares de nós em
menos de 0,3 s) mesmo com o comando de `pursuit()` limitado em taxa. O que resolveu **não** foi
mexer no controlador: foi **não** reduzir `ixx`/`iyy`/`izz` do JSBSim (ficam os do c310, cujas
tabelas aerodinâmicas foram calibradas para essa inércia) — só `emptywt` cai (~15×), o que já basta
para "mais carga G" (G = sustentação / peso) sem desestabilizar a dinâmica rotacional. Ver
`CLAUDE.md` para a lista completa de armadilhas desta demo (envelope de lançamento, `updateTC()`
chamado 4× por frame, nome de fábrica `StoresMgr`, comentário XML sem `--`).

## Testes

`tests/domain/` cobre só `domain::pursuit()` — pura, sem MIXR, sem JSBSim, no mesmo espírito da
camada `domain` do `flight`. Não há teste de `xmissile::GuidedMissile` isolado (a classe MIXR):
ela é exercitada de ponta a ponta pelo cenário de demo
(`src/poc/dis/single-thread/configs/scenario_missile_demo.edl.in`), a mesma prova que os testes de
`tests/scenario/` do host já fazem para os outros modelos.
