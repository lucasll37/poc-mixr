# `shared/data/terrain/srtm/` — tiles SRTM

Cada `.hgt.gz` aqui é um tile SRTM de 1°×1°, nome = canto SW (`S<lat>W<lon>.hgt`,
convenção USGS). `.gz` é o que fica versionado (12 MB o real, ~2 MB cada sintético);
o `.hgt` descomprimido é gerado em disco sob demanda por `app::ensureTerrainData()`/
`app::ensureAllTerrainTiles()` (`app/src/app/TerrainData.cpp`) e é gitignored.

| tile | origem | resolução | cobertura (lat/lon) |
|---|---|---|---|
| `S23W043` | **REAL** — NASA SRTM1, recuperado do histórico do git (era da `poc/05-formation-flight`) | 1 arco-seg (~30 m), 3601×3601 | -23 a -22, -43 a -42 |
| `S22W043` | **SINTÉTICO** | 3 arco-seg (~90 m), 1201×1201 | -22 a -21, -43 a -42 |
| `S23W044` | **SINTÉTICO** | 3 arco-seg (~90 m), 1201×1201 | -23 a -22, -44 a -43 |
| `S22W044` | **SINTÉTICO** | 3 arco-seg (~90 m), 1201×1201 | -22 a -21, -44 a -43 |

## Por que três tiles são sintéticos, não SRTM real

Investigado antes de vendorizar qualquer coisa: a distribuição oficial de SRTM1 da
NASA (LP DAAC / Earthdata) hoje exige login (verificado buscando a documentação
atual — `dwtkns.com/srtm30m` confirma "As of 2026, NASA distributes SRTM via the
Earthdata Cloud"); a antiga rota sem login (`dds.cr.usgs.gov`) redireciona para lá.
Não foi encontrada, sem me arriscar a inventar/adivinhar URL, uma fonte confiável
que sirva `.hgt` no formato binário exato que `SrtmHgtFile` exige (big-endian
int16, tamanho em bytes validado — ver `SrtmHgtFile.hpp`) sem exigir conta.

O que existe: `elevationAt(lat, lon)` — soma de senos de poucas frequências,
função PURA de lat/lon absolutos (não por-tile), então dois tiles gerados por ela
são automaticamente contínuos na fronteira compartilhada, sem *stitching*
explícito. Faixa de saída (0–1800 m) escolhida pra não destoar do tile real vizinho
(medido: 0–1960 m). **Não é elevação de verdade** — é só o suficiente pra provar
que o mecanismo de múltiplos tiles (`app/TerrainTiles.hpp`) funciona: carrega mais
de um `Terrain`, escolhe o certo por coordenada, mostra terreno contínuo cruzando
a fronteira entre `S23W043` (real) e os vizinhos sintéticos na aba Mapa do `app`.

Script gerador (não versionado, de uso único): descrito no CLAUDE.md, seção do
`app` — refaça-o se precisar gerar mais tiles sintéticos no mesmo estilo.

## Para acrescentar cobertura de verdade depois

Basta soltar mais `.hgt.gz` REAIS aqui (mesma convenção de nome) — nenhuma mudança
de código é necessária: `ensureAllTerrainTiles()` descompacta qualquer `.hgt.gz`
que encontrar, e o `app` carrega qualquer `.hgt` válido que achar no diretório.
