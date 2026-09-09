# `shared/data/terrain/srtm/` — tiles de elevação

**Cinco tiles versionados** (o cenário de demonstração e sua vizinhança imediata) e — em disco,
fora do git — a cobertura que `scripts/fetch_srtm.sh` tiver baixado. O `.hgt` descompactado é
gitignored e recriado sob demanda (`app::makeTerrainSampler()`); o `.gz` também é gitignored por
padrão, com as cinco exceções nomeadas no `.gitignore` da raiz. Nome de 11 caracteres, convenção SRTM
(`S23W043.hgt` = canto sudoeste do tile, 23°S 43°O), lida por posição fixa em
`SrtmHgtFile::determineSrtmInfo()` e replicada aqui (só para ESCOLHER o tile certo por
coordenada, não para reimplementar o parser) em `app/TerrainQuery.cpp`.

| tile | origem | formato | tamanho (`.hgt`) |
|---|---|---|---|
| `S23W043` | **real** — Serra do Mar (RJ), recuperado do histórico do git (era da poc/05) | SRTM1 (3601×3601) | 25.934.402 bytes |
| `S23W042` | **real** — vizinho a leste (Atlântico + litoral de Cabo Frio), baixado do espelho aberto da AWS | SRTM1 (3601×3601) | 25.934.402 bytes |
| `S22W043` | **real** — vizinho ao norte (espelho aberto da AWS) | SRTM1 (3601×3601) | 25.934.402 bytes |
| `S23W044` | **real** — vizinho a oeste, a serra (espelho aberto da AWS) | SRTM1 (3601×3601) | 25.934.402 bytes |
| `S22W044` | **real** — vizinho a noroeste (espelho aberto da AWS) | SRTM1 (3601×3601) | 25.934.402 bytes |

## Cobertura da família `A4-*DOF`, medida

A figura-de-oito de 20 steerpoints que `A4-6DOF`/`A4-4DOF`/`A4-3DOF` voam cabe **inteira dentro de
`S23W043`** — não é estimativa: a caixa dos próprios steerpoints, com 6 km de folga para o
*overshoot* de curva, dá `lat -22,834 .. -22,098` e `lon -42,937 .. -42,023`, e uma varredura de
160.801 pontos sobre essa pegada não achou **nenhum** ponto fora do tile nem **nenhum** *void*
(elevação de −1 a 2.232 m, mediana 306 m). A telemetria de uma volta (`altMslM − altAglM`, 118.000
frames) bate com a leitura direta do `.hgt` com **0,00 m** de diferença média — o
`interpolateTerrain: true` do cenário casa com a bilinear.

**`S23W043` é o tile real, verificado**: `md5` byte a byte idêntico ao servido pelo espelho oficial
(`s3.amazonaws.com/elevation-tiles-prod/skadi/S23/S23W043.hgt.gz`).

**Por que `S23W042` foi acrescentado**: a folga da rota até a borda **leste** do tile é de apenas
**2,4 km** (a menor das quatro; norte 10,9 km, oeste 6,5 km, sul 18,5 km) — e além dela não havia
tile nenhum, nem sintético. Cruzar essa borda seria uma falha **silenciosa**, não um erro:
`Player::updateElevation()` ignora o retorno de `getElevation()` (`Player.cpp:3205-3206`), deixa
`el` em `0.0` e ainda liga `tElevValid = true` — ou seja, o anti-CFIT passaria a raciocinar sobre
um "terreno" ao nível do mar sem avisar ninguém. Custa 3,0 MB (é 72,9% oceano, comprime muito).
Confirmado rodando `app::makeTerrainSampler()` de produção, com controle: um ponto 10 km além da
borda leste devolve `SEM DADO` sem o tile e `elev = 29,7 m` com ele, enquanto os pontos sob a rota
saem idênticos nos dois casos.

## Os três vizinhos deixaram de ser sintéticos

`S22W043`/`S23W044`/`S22W044` eram **gerados** (soma de senos sobre lat/lon absolutos, faixa
0–1800 m) porque a premissa da época era que não havia fonte aberta de `.hgt` real: a NASA passou a
exigir Earthdata login e o CGIAR-CSI serve GeoTIFF/ASCII Grid, que precisaria de conversão.

**A premissa estava incompleta.** O espelho *Terrain Tiles* da AWS Open Data serve `.hgt.gz` SRTM1
real, **sem login**, no formato binário exato que `SrtmHgtFile` exige:

```
https://s3.amazonaws.com/elevation-tiles-prod/skadi/<S23>/<S23W043>.hgt.gz
```

Os três foram substituídos por dado real. A assinatura do sintético era visível ao medir: os três
tinham máximo **exatamente 1800 m** (o teto da função geradora), enquanto os reais dão 1417, 2237 e
1780 m. **Não há mais nenhum tile sintético neste repositório.**

## Cobertura do Brasil — em disco, fora do git

`scripts/fetch_srtm.sh --brasil` baixa a caixa do Brasil inteira: **1.600 tiles, ~12 GB** de `.gz`
(Chuí a Monte Caburaí, Serra do Divisor à Ponta do Seixas). Eles **não são versionados** — ficariam
para sempre no histórico do git e inviabilizariam o clone — e não precisam ser: o espelho é público
e o script é idempotente, então rodá-lo de novo reconstrói tudo, pulando o que já está em disco.

Duas mudanças no `./app` foram **necessárias** para esse volume ser utilizável, as duas medidas
antes e depois:

1. **`app/TerrainQuery.cpp` carregava TODO `.hgt` da pasta na primeira consulta e nunca liberava
   nenhum.** Com 4 tiles isso era invisível; com 1.600 seriam **~41,5 GB de RAM** na primeira vez
   que alguém abrisse a aba Mapa. Hoje ele mantém um **índice** (só nome e canto SW — um `readdir`,
   sem abrir dado nenhum), carrega o tile na primeira consulta que cai dentro dele, e mantém no
   máximo `kMaxResidentTiles` (12, ~311 MB) residentes, despejando por LRU. A busca virou um
   `std::map` por célula inteira em vez de varredura linear: a aba Mapa faz ~3.200 consultas por
   redesenho, e 1.600 tiles × 3.200 seriam 5,1 milhões de testes de caixa por quadro.
   **Medido, com 700 tiles em disco:** pico de RSS de **332 MB**, o mesmo numa varredura de 4° e
   numa de 10° (~100 tiles cruzados). **A/B contra a versão anterior**, sob teto de 2 GB
   (`ulimit -v`, para falhar em segurança em vez de derrubar a máquina): a antiga morre com
   `std::bad_alloc`; a nova completa em 332 MB.
2. **`app::ensureAllTerrainTiles()` descompactava a pasta inteira na partida** — 41,5 GB de `.hgt` e
   minutos de espera, para tiles que aquela execução nunca consultaria. Agora tem teto: acima de 16
   `.gz` ela não descompacta nada e avisa, deixando o trabalho para o caminho preguiçoso acima.
   **Medido:** partida do `./app` com 1.600 tiles em disco em **0,29 s**, 70 MB de pico.

Nenhuma das duas muda resultado de simulação: o `terrain:` do EDL nomeia **um** tile, e quem o lê é
o `Player::updateElevation()` nativo. Os dumps `-deterministic` de `A4-3DOF`/`A4-4DOF` saíram
byte-idênticos aos de antes da mudança, e `make test` fecha **66/66**.

## Como acrescentar cobertura real depois

Nenhuma mudança de código é necessária — só soltar mais `.hgt`/`.hgt.gz` válidos nesta pasta:

1. Rodar `scripts/fetch_srtm.sh`, que já cuida de fonte, nome, verificação de integridade
   (`gzip -t`) e retomada (pula o que já existe):

   ```bash
   scripts/fetch_srtm.sh S23W042                  # tiles nomeados
   scripts/fetch_srtm.sh --bbox -25 -20 -46 -40   # uma caixa de lat/lon
   scripts/fetch_srtm.sh --brasil                 # o Brasil inteiro (~12 GB)
   ```
2. Nomear com a convenção de 11 caracteres (`S23W043.hgt`, `N05W060.hgt`, etc.) e comprimir com
   `gzip` (`app::ensureAllTerrainTiles()` só descompacta `.gz`; um `.hgt` cru pode ser deixado
   sem compressão, mas não é o padrão desta pasta).
3. Confirmar o tamanho exato em bytes contra a tabela de `SrtmHgtFile::determineSrtmInfo()`
   (2.884.802 = SRTM3, 25.934.402 = SRTM1) — qualquer outro tamanho falha com *"ERROR in
   determining SRTM type"* sem dizer qual arquivo, então vale conferir antes de rodar.

`app::makeTerrainSampler()`/`tileRepository()` (`app/TerrainQuery.cpp`) escaneiam a pasta inteira
na primeira consulta de cada execução — um tile novo aparece automaticamente, tanto no
`Player::updateElevation()` nativo (que só enxerga o tile único declarado em `terrain:` no EDL do
cenário) quanto na vista de Mapa do `./app` (que enxerga todos).
