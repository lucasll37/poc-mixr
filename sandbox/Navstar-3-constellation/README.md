# Navstar-3-constellation — quatro planos orbitais, o mesmo modelo

Reaproveita o modelo `models/others/Navstar-3` (ver
[`src/poc/navstar3-orbit`](../../src/poc/navstar3-orbit/README.md) para a demonstração de **um**
satélite só) com **quatro instâncias**, uma por plano orbital — o que a poc deixou de propósito
fora de escopo (ver `models/others/Navstar-3/docs/ARCHITECTURE.md`, seção "O que fica de fora":
*"o modelo aceita múltiplas instâncias... mas o cenário de produção demonstra só UM satélite"*).
Este sandbox é exatamente essa extensão — e é **só EDL**: os elementos orbitais
(`altitude`/`inclination`/`raan`/`argLat0`) já eram slots por instância de `( Navstar3BtBehavior
)`, nenhuma linha de C++ precisou mudar.

```bash
./build/app/src/app -folder ./sandbox -scenario Navstar-3-constellation                       # tempo real, Tacview 1234
./build/app/src/app -folder ./sandbox -scenario Navstar-3-constellation -deterministic 1200
```

## O desenho

Quatro `( SpaceVehicle )`, mesma altitude (~20.180 km)/inclinação (55°)/fase inicial
(`argLat0: 0`), espalhados por **`raan`** (ascensão reta do nodo ascendente): `navstar1`=0°,
`navstar2`=90°, `navstar3`=180°, `navstar4`=270° — quatro planos orbitais diferentes, o mesmo
padrão de "um satélite por plano" que uma constelação GPS real usa (6 planos reais; 4 aqui, o
suficiente para ver planos distintos sem inflar o cenário). A mesma direção de Sol fixa de
`navstar3-orbit` (`sunRightAscension: 180`) — o Sol é um só na vida real, os quatro compartilham
a mesma direção.

`initLatitude`/`initLongitude` de cada satélite (0°/90°/180°/-90°) já batem com a posição que o
propagador orbital calcularia no primeiro ciclo (`u=0` em cada plano cai exatamente em
`lon=raan`) — não há descontinuidade entre a posição inicial declarada e a primeira posição
calculada.

## Medido rodando (600 frames, 12 s simulados, `-deterministic`)

1. **Os quatro planos cruzam a sombra em momentos diferentes** — a prova mais interessante deste
   sandbox: no MESMO frame, `navstar1` (`raan=0`) sai `bt=ECLIPSE` do início ao fim da janela
   medida, enquanto `navstar2`/`navstar3`/`navstar4` (planos a 90°/180°/270°) saem `bt=SUNLIT` —
   quatro instâncias da MESMA classe C++, decidindo de forma diferente porque a geometria
   (posição real de cada plano em relação à direção fixa do Sol) é diferente. Confirma que
   `domain::sunState()` é avaliado por instância, sem nenhum estado compartilhado entre elas.
2. **Altitude HAE constante nos quatro** — `alt=20180000.000000000` em toda amostra, todo
   satélite, mesmo padrão de `navstar3-orbit`.
3. **Posições avançando de forma independente e coerente com o `raan` de cada um** — a
   coordenada local `e` (leste) diverge claramente entre os quatro desde o primeiro frame
   amostrado (`navstar1 e≈276`, `navstar2 e≈10.001.212`, `navstar3 e≈-20.001.188`,
   `navstar4 e≈-10.000.388`, em `frame=300`) — os quatro planos estão visivelmente separados,
   não sobrepostos.
4. **`dec` avança na mesma taxa que `frame`, nos quatro** — `dec=601` em `frame=600` para cada
   satélite (offset de 1 pelo warm-up tick, mesmo comportamento já documentado para as demais
   pocs/cenários deste repositório).
5. **Achado esperado, não um bug**: o relatório de metaobjetos do `-deterministic`
   (`meta=Navstar3Action count=...`) sai com um valor **negativo** quando os quatro satélites
   decidem em paralelo em threads diferentes do pool T/C — `mixr::base::MetaObject::count` é
   `int` cru, não atômico (armadilha já documentada no `CLAUDE.md` raiz, seção "Testes
   automatizados", para o modelo A-4/`FlightAgentTC`). Não afeta o **estado da simulação** (só a
   estatística de contagem de instâncias do próprio dump) — confirmado pelo item 6 abaixo.
6. **Determinismo, com os quatro decidindo em paralelo**: `tests/determinism/
   check_determinism.sh` sobre 1200 frames — dumps byte-idênticos com 1, 2 e 4 threads T/C (mais
   a repetição de 4), uma decisão por frame por satélite, nas três configurações — a corrida no
   contador de `MetaObject` (item 5) não contamina a posição/rótulo de nenhum satélite.
7. **Tacview**: `colorMap` distingue os quatro por cor (`navstar1` azul, `navstar2` amarelo,
   `navstar3` ciano, `navstar4` vermelho) — os quatro planos ficam visualmente distinguíveis sem
   precisar abrir o painel de detalhe de cada um.

## Ler também

- [`../../models/others/Navstar-3/docs/ARCHITECTURE.md`](../../models/others/Navstar-3/docs/ARCHITECTURE.md)
  — todas as decisões de design do modelo
- [`../../src/poc/navstar3-orbit/README.md`](../../src/poc/navstar3-orbit/README.md) — a
  demonstração canônica de um satélite só, com a maior parte do "medido rodando" detalhado
