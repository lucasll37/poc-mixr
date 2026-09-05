# `shared/data/terrain/srtm/` — tiles de elevação

Quatro tiles `.hgt` (comprimidos em `.gz`, o único formato versionado — `.hgt` descompactado é
gitignored e recriado em disco pela primeira execução, via `app::ensureTerrainData()`/
`app::ensureAllTerrainTiles()`, ver `app/TerrainData.cpp`). Nome de 11 caracteres, convenção SRTM
(`S23W043.hgt` = canto sudoeste do tile, 23°S 43°O), lida por posição fixa em
`SrtmHgtFile::determineSrtmInfo()` e replicada aqui (só para ESCOLHER o tile certo por
coordenada, não para reimplementar o parser) em `app/TerrainQuery.cpp`.

| tile | origem | formato | tamanho (`.hgt`) |
|---|---|---|---|
| `S23W043` | **real** — Serra do Mar (RJ), recuperado do histórico do git (era da poc/05) | SRTM1 (3601×3601) | 25.934.402 bytes |
| `S22W043` | **sintético** — vizinho ao norte | SRTM3 (1201×1201) | 2.884.802 bytes |
| `S23W044` | **sintético** — vizinho a oeste | SRTM3 (1201×1201) | 2.884.802 bytes |
| `S22W044` | **sintético** — vizinho a noroeste | SRTM3 (1201×1201) | 2.884.802 bytes |

## Por que três dos quatro são sintéticos

O pedido original era cobertura de elevação para a América do Sul inteira — centenas de tiles
SRTM1, vários GB, ordens de grandeza acima do que faz sentido vendorizar neste repositório (ver
CLAUDE.md, seção `./app`, "décima passada" e "décima primeira passada", para a decisão completa).
A fonte oficial da NASA para SRTM1 hoje exige login via Earthdata Cloud (o antigo espelho sem
login, `dds.cr.usgs.gov`, redireciona para lá); CGIAR-CSI é aberto mas serve GeoTIFF/ASCII Grid,
não o binário `.hgt` (int16 big-endian, tamanho validado em bytes) que `SrtmHgtFile` exige.

A escolha foi construir a **infraestrutura multi-tile de verdade** (`app::makeTerrainSampler()`
carrega TODO `.hgt` válido encontrado nesta pasta, escolhe por coordenada, sem lista fixa de
nomes) e vendorizar só um punhado de tiles de exemplo — em vez de prometer cobertura continental
e falhar em silêncio, ou não entregar nada.

Os três vizinhos (`S22W043`/`S23W044`/`S22W044`) foram **gerados**, não baixados: uma função
`elevationAt(lat, lon)` — soma de senos de poucas frequências, deliberadamente simples — é
avaliada sobre lat/lon **absolutos** (não por-tile), então dois tiles quaisquer gerados por ela
saem automaticamente contínuos na fronteira compartilhada, sem nenhum código de *stitching*.
Faixa de saída escolhida em 0–1800 m para não destoar do tile real vizinho (medido: 0–1960 m).
Deliberadamente em **SRTM3** (resolução menor que o tile real, 1201×1201 contra 3601×3601) —
duas razões: não fingir uma precisão que o dado não tem, e manter o repositório pequeno (~1,9 MB
por `.gz` contra os ~12,4 MB que um SRTM1 sintético equivalente pesaria).

**Isto não é dado de elevação real.** Não usar estes três tiles para nada que dependa da forma
real do terreno fora do raio do cenário de demonstração (Serra do Mar, RJ) — eles existem só
para exercitar o mecanismo de múltiplos tiles (escolha por coordenada, continuidade de borda,
sombreamento/curvas de nível na vista de Mapa do `./app`) sem inflar o repositório.

## Como acrescentar cobertura real depois

Nenhuma mudança de código é necessária — só soltar mais `.hgt`/`.hgt.gz` válidos nesta pasta:

1. Baixar o tile SRTM1 ou SRTM3 de uma fonte que sirva o formato binário `.hgt` (não
   GeoTIFF/ASCII Grid — precisaria de conversão antes).
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
