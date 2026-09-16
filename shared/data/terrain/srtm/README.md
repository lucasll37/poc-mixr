# `shared/data/terrain/srtm/` — tiles de elevação

Cinco tiles versionados (cenário de demonstração + vizinhança imediata); o resto da cobertura
fica fora do git (ver `.gitignore` desta pasta), baixável com `scripts/fetch_srtm.sh`.

Nome de 11 caracteres, convenção SRTM (`S23W043.hgt` = canto sudoeste, 23°S 43°O).

| tile | origem | formato | tamanho (`.hgt`) |
|---|---|---|---|
| `S23W043` | real — Serra do Mar (RJ) | SRTM1 (3601×3601) | 25.934.402 bytes |
| `S23W042` | real — vizinho a leste | SRTM1 (3601×3601) | 25.934.402 bytes |
| `S22W043` | real — vizinho ao norte | SRTM1 (3601×3601) | 25.934.402 bytes |
| `S23W044` | real — vizinho a oeste | SRTM1 (3601×3601) | 25.934.402 bytes |
| `S22W044` | real — vizinho a noroeste | SRTM1 (3601×3601) | 25.934.402 bytes |
