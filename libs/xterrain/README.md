# `libs/xterrain` — terreno multi-tile (SRTM), com carga sob demanda

`( MultiTileTerrain )` serve **vários** tiles SRTM (`.hgt`/`.hgt.gz`, um por grau de lat/lon) a
partir de um único diretório — alternativa ao `( SrtmHgtFile )` nativo do MIXR, que só serve **um**
tile, para cenários cuja área de voo excede 1×1 grau (ex.: voo livre por qualquer parte do Brasil,
com `scripts/fetch_srtm.sh --brasil` chegando a ~1600 tiles em disco).

## Como se usa

No lugar de:

```
terrain: ( SrtmHgtFile
   path: "./shared/data/terrain/srtm/"
   file: "S23W043.hgt"
)
```

```
terrain: ( MultiTileTerrain
   dir: "./shared/data/terrain/srtm/"
)
```

Sem `file:` — o diretório inteiro é a fonte. `getElevation(lat, lon)` resolve, a cada chamada, a
célula (`floor(lat)`, `floor(lon)`) que cobre o ponto e delega para um `SrtmHgtFile` interno
daquela célula, carregado (e descomprimido, se preciso) na primeira consulta. Acrescentar
cobertura depois é só soltar mais `.hgt`/`.hgt.gz` na pasta — nenhuma mudança de EDL.

## Por que não dava para usar o que já existia no MIXR

- `SrtmHgtFile`/`DtedFile` (via `DataFile`) carregam **um** array em memória, o arquivo inteiro —
  não têm noção de "vários tiles".
- `QuadMap` combina até 4 tiles filhos, mas por `components:` do EDL (estático, declarado um a um)
  — incompatível com um diretório de ~1600 arquivos descobertos em runtime. Além disso o `clone()`
  dele (`IMPLEMENT_ABSTRACT_SUBCLASS`) devolve sempre `nullptr`, o que faz `WorldModel::copyData()`
  estourar num `unref()` sem checagem — `MultiTileTerrain` usa `IMPLEMENT_SUBCLASS` de propósito,
  com `clone()`/`copyData()` de verdade (o clone nasce com cache vazio, recarrega sob demanda).

## Cache: tamanho e por que ele importa

`kMaxResidentTiles` (12, ~310 MB em SRTM1) é o teto de tiles **residentes** ao mesmo tempo — cada
tile novo consultado entra no cache; passado o teto, o menos recentemente usado é despejado
(`unref()`'d). Isso é **transparente por ponto**: não existe "player atribuído a um tile", cada
chamada de `getElevation()` resolve sozinha, então múltiplos players em regiões diferentes mantêm
tiles residentes distintos sem conflito, **enquanto o número de tiles distintos em uso não exceder
o teto**.

**Risco real de dimensionamento**: se os players de um cenário estiverem espalhados por mais tiles
distintos do que o cache aguenta (ex. 20 aeronaves em 20 regiões diferentes com teto 12), o tile
menos usado é ejetado a cada novo, e se ainda estiver em uso é recarregado (gunzip + parse) no
próximo frame de fundo — sempre. Isso não corrompe o resultado (o valor devolvido continua
correto — só custa I/O repetido). Ajuste `kMaxResidentTiles` (constante pública da classe) ao
padrão de uso real do cenário.

## Concorrência — por que o mutex aqui é obrigatório, não cautela

`Player::updateElevation()` roda em `updateData()` (fase de fundo), e
`mixr::simulation::Simulation::updateData()` despacha os players por um pool de até `numBgThreads`
threads OS reais quando esse slot é > 1 — e os cenários de produção deste repositório já usam
`numBgThreads: 2` por padrão. Como todos os players compartilham o **mesmo** `WorldModel::terrain`,
mais de uma thread pode chamar `getElevation()` no mesmo objeto ao mesmo tempo — e nada no
framework nativo (`Terrain`/`DataFile`/`SrtmHgtFile`) serializa isso por conta própria. Por isso o
índice/cache/LRU desta classe é protegido por `std::mutex` de ponta a ponta, diferente do mecanismo
irmão em `app/TerrainQuery.cpp` (a vista de terreno do Mapa do `./app`), que só tem **um**
chamador — a thread de UI do FTXUI — e onde o mutex é mais cautela do que necessidade estrutural.

## O que fica de fora, deliberadamente

- `getMinElevation()`/`getMaxElevation()` (herdados de `Terrain`) não são mantidos — não são
  virtuais (só devolvem campos privados da própria base) e nada na física/domínio deste projeto os
  consulta.
- `getElevations()` (plural, usado só por `Terrain::targetOcculting()` — mascaramento de radar por
  terreno) tem implementação correta mas simples: delega para `getElevation()` ponto a ponto:
  nenhum cenário deste repositório exercita esse caminho hoje.

## Testes

`tests/domain/test_xterrain_multi_tile.cpp` — tiles SRTM3 sintéticos (uniformes, gerados em
`::testing::TempDir()`, sem depender dos tiles reais de `shared/data/terrain/srtm/`), cobrindo:
resolução por célula com players em tiles diferentes, travessia de fronteira sem lógica especial,
despejo de LRU acima do teto (e recarga correta do tile despejado), concorrência real (várias
threads chamando `getElevation()` no mesmo objeto), `clone()` com cache vazio, e o caminho de
descompressão sob demanda (`.hgt.gz`-only).
