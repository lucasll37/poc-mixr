# `tools/data/` — dado vendorizado para ferramentas de `tools/`

**`world_countries_110m.json`** — contorno de 177 países/territórios, usado só por
`tools/plot_terrain_coverage.py` para desenhar o mapa-mundi de contexto e o recorte de cada
região com cobertura SRTM. Não é dado de simulação (não fica em `shared/data/`, que é
vendorizado para os cenários MIXR rodarem) — é vendorizado aqui porque só esta ferramenta o usa.

**Origem**: [Natural Earth](https://www.naturalearthdata.com/) 1:110m Cultural Vectors, Admin 0
— Countries (a resolução mais grosseira que o projeto publica, apropriada para um mapa em escala
de globo/continente). Domínio público — "No permission is needed to use Natural Earth. Crediting
the authors is unnecessary." Baixado de
`https://raw.githubusercontent.com/nvkelso/natural-earth-vector/master/geojson/ne_110m_admin_0_countries.geojson`
(o espelho GeoJSON oficial do próprio projeto Natural Earth).

**Transformação aplicada** (script de uso único, não versionado — receita abaixo para reproduzir):
de cada feature, mantido só `NAME` e a geometria; coordenadas arredondadas para 2 casas decimais
(~1,1 km de precisão — mais que suficiente numa tela onde 1 px representa vários km); só o anel
EXTERIOR de cada polígono é mantido (buracos internos — enclaves como Lesoto dentro da África do
Sul, San Marino dentro da Itália — não aparecem em escala de mapa-múndi/regional e não valem o
peso extra). `MultiPolygon` vira uma lista de anéis por país (arquipélagos/ilhas continuam
aparecendo, cada um seu próprio anel). De 838.726 bytes (GeoJSON original, com dezenas de campos
de metadado por país) para 159.027 bytes.

Formato: `[{"name": str, "rings": [[[lon, lat], ...], ...]}, ...]` — sem `"type"`/`"properties"`/
`"geometry"` do GeoJSON, só o que `plot_terrain_coverage.py` de fato consome.

Para reproduzir (ou atualizar para uma versão mais nova do Natural Earth):

```bash
curl -sS -o /tmp/ne110m.geojson \
  https://raw.githubusercontent.com/nvkelso/natural-earth-vector/master/geojson/ne_110m_admin_0_countries.geojson
python3 - <<'PY'
import json
d = json.load(open("/tmp/ne110m.geojson", encoding="utf-8"))
def round_ring(ring, nd=2):
    return [[round(lon, nd), round(lat, nd)] for lon, lat in ring]
out = []
for feat in d["features"]:
    name = feat["properties"].get("NAME") or feat["properties"].get("ADMIN") or "?"
    geom = feat["geometry"]
    polys = [geom["coordinates"]] if geom["type"] == "Polygon" else geom["coordinates"] if geom["type"] == "MultiPolygon" else []
    rings = [round_ring(p[0]) for p in polys if p and len(p[0]) >= 3]
    if rings:
        out.append({"name": name, "rings": rings})
out.sort(key=lambda c: c["name"])
open("tools/data/world_countries_110m.json", "w", encoding="utf-8").write(
    json.dumps(out, separators=(",", ":"), ensure_ascii=False) + "\n"
)
PY
```
