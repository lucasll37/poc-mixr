#!/usr/bin/env python3
"""Varre `shared/data/terrain/` (ou outro diretorio passado por `--dir`) e
desenha, em SVG, quais regioes do globo tem cobertura de elevacao SRTM em
disco -- um mapa-mundi de contexto (com o contorno real dos paises, vendorizado
em `tools/data/world_countries_110m.json` -- ver o README ao lado) mais um
"zoom" por 1x1 grau para cada regiao contigua encontrada.

So' entende a convencao SRTM que este repositorio de fato usa: arquivos
`<tag>.hgt`, `<tag>.hgt.gz` ou o
residuo de download incompleto `<tag>.hgt.gz.parcial`, com `<tag>` no formato
de 11 caracteres `[NS]DD[EW]DDD.hgt` (ex.: `S23W043.hgt` = canto sudoeste a
23S 43O). O nome e' lido por POSICAO (ultimos 11 caracteres antes de
qualquer `.gz`/`.parcial`), a mesma logica de
`app/src/app/TerrainQuery.cpp::parseSwCorner()` e de
`SrtmHgtFile::determineSrtmInfo()` -- replicada aqui em Python so' para
CLASSIFICAR o arquivo, nunca para ler o dado de elevacao em si. Um arquivo
que nao bate com essa convencao (ex.: `README.md`) e' listado como
"ignorado", nunca adivinhado.

Sem dependencia nenhuma alem da biblioteca padrao -- mesma convencao dos
outros scripts de `tools/` (nenhum usa matplotlib/numpy). A resolucao
(SRTM1 x SRTM3) e' inferida sem descomprimir nada: os ultimos 4 bytes de um
`.gz` sao o campo ISIZE do gzip (RFC 1952 SS2.3.1, little-endian, modulo
2**32) -- exatamente o tamanho do `.hgt` descomprimido, e todo tile SRTM
fica muito abaixo do teto de 4 GiB onde esse campo deixaria de ser exato.

Uso:
    python3 tools/plot_terrain_coverage.py
    python3 tools/plot_terrain_coverage.py --dir shared/data/terrain --open
    python3 tools/plot_terrain_coverage.py --dir /caminho/qualquer --out /tmp/cobertura.svg

    make terrain-coverage   # o mesmo, com --open (gera e ja mostra o SVG)

Saida (default, gitignorada -- ver `build*/` no `.gitignore` da raiz):
    build/terrain-coverage/coverage.svg   -- o mapa
    build/terrain-coverage/coverage.json  -- o mesmo levantamento, em dado
"""
from __future__ import annotations

import argparse
import functools
import json
import math
import os
import struct
import subprocess
import sys
import textwrap
from dataclasses import dataclass, field
from datetime import datetime, timezone
from enum import Enum
from pathlib import Path
from typing import Optional

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_DIR = REPO_ROOT / "shared" / "data" / "terrain"
DEFAULT_OUT_DIR = REPO_ROOT / "build" / "terrain-coverage"
COUNTRIES_PATH = Path(__file__).resolve().parent / "data" / "world_countries_110m.json"

# Tamanho exato do .hgt DESCOMPRIMIDO que SrtmHgtFile::determineSrtmInfo()
# aceita (contexts/src/mixr/src/terrain/srtm/SrtmHgtFile.cpp:154-167) --
# qualquer outro tamanho falha no proprio app com "ERROR in determining
# SRTM type".
SRTM1_SIDE = 3601
SRTM3_SIDE = 1201
SRTM1_BYTES = SRTM1_SIDE * SRTM1_SIDE * 2
SRTM3_BYTES = SRTM3_SIDE * SRTM3_SIDE * 2

KM_PER_DEG_LAT = 111.32  # aproximado (WGS84 varia pouco com a latitude)


class Status(Enum):
    """Classificacao de uma celula de 1x1 grau -- ordem tambem serve de
    PRIORIDADE ao mesclar mais de um arquivo na mesma celula (ex.: um
    `.hgt` ja descomprimido ao lado do `.hgt.gz` de origem): o melhor
    estado encontrado e' o que decide a cor da celula, mas nenhuma forma
    e' descartada -- todas ficam registradas em `CellCoverage.forms`."""

    SRTM1 = "SRTM1 (1 arco-seg, 3601x3601, ~30 m)"
    SRTM3 = "SRTM3 (3 arco-seg, 1201x1201, ~90 m)"
    UNKNOWN_SIZE = "tamanho inesperado"
    INCOMPLETE = "download incompleto (.parcial)"


STATUS_PRIORITY = [Status.SRTM1, Status.SRTM3, Status.UNKNOWN_SIZE, Status.INCOMPLETE]

STATUS_COLOR = {
    Status.SRTM1: "#1b7a3d",
    Status.SRTM3: "#8bc34a",
    Status.UNKNOWN_SIZE: "#e53935",
    Status.INCOMPLETE: "#f9a825",
}


@dataclass
class CellCoverage:
    sw_lat: int
    sw_lon: int
    status: Status = Status.INCOMPLETE
    size_bytes: Optional[int] = None
    forms: set[str] = field(default_factory=set)
    paths: list[str] = field(default_factory=list)
    stale_cache: bool = False
    hgt_size: Optional[int] = None
    gz_isize: Optional[int] = None

    @property
    def name(self) -> str:
        ns = "S" if self.sw_lat < 0 else "N"
        ew = "W" if self.sw_lon < 0 else "E"
        return f"{ns}{abs(self.sw_lat):02d}{ew}{abs(self.sw_lon):03d}"


@dataclass
class ScanResult:
    root: Path
    cells: dict[tuple[int, int], CellCoverage]
    ignored: list[str]


@dataclass
class Cluster:
    """Um grupo de celulas 8-conectadas -- a nocao de "uma regiao contigua
    do globo com cobertura", nao so' uma celula isolada."""

    cells: list[tuple[int, int]]

    @property
    def min_lat(self) -> int:
        return min(lat for lat, _ in self.cells)

    @property
    def max_lat(self) -> int:
        return max(lat for lat, _ in self.cells)

    @property
    def min_lon(self) -> int:
        return min(lon for _, lon in self.cells)

    @property
    def max_lon(self) -> int:
        return max(lon for _, lon in self.cells)


@dataclass
class Country:
    name: str
    rings: list[list[tuple[float, float]]]  # cada anel: [(lon, lat), ...]
    bbox: tuple[float, float, float, float]  # (min_lon, min_lat, max_lon, max_lat)


@functools.lru_cache(maxsize=1)
def load_countries() -> list[Country]:
    """Le `tools/data/world_countries_110m.json` (Natural Earth 110m, dominio
    publico -- proveniencia completa em `tools/data/README.md`). Devolve `[]`
    em silencio se o arquivo nao existir -- o mapa ainda funciona sem contorno
    de pais nenhum, so' fica sem esse contexto (mesma degradacao graciosa que
    o resto desta ferramenta usa para dado ausente)."""
    if not COUNTRIES_PATH.is_file():
        return []
    with COUNTRIES_PATH.open("r", encoding="utf-8") as f:
        raw = json.load(f)
    countries = []
    for entry in raw:
        rings = [[(float(lon), float(lat)) for lon, lat in ring] for ring in entry["rings"]]
        lons = [lon for ring in rings for lon, _ in ring]
        lats = [lat for ring in rings for _, lat in ring]
        countries.append(Country(name=entry["name"], rings=rings,
                                   bbox=(min(lons), min(lats), max(lons), max(lats))))
    return countries


def bbox_overlaps(a: tuple[float, float, float, float], b: tuple[float, float, float, float]) -> bool:
    a_min_lon, a_min_lat, a_max_lon, a_max_lat = a
    b_min_lon, b_min_lat, b_max_lon, b_max_lat = b
    return a_min_lon <= b_max_lon and a_max_lon >= b_min_lon and a_min_lat <= b_max_lat and a_max_lat >= b_min_lat


def clip_ring_to_bbox(ring: list[tuple[float, float]], min_lon: float, min_lat: float,
                        max_lon: float, max_lat: float) -> list[tuple[float, float]]:
    """Sutherland-Hodgman: recorta um poligono (fechado, sentido qualquer)
    contra uma janela retangular alinhada aos eixos -- os quatro semiplanos
    (esquerda/direita/baixo/cima), um de cada vez. Correto para QUALQUER
    poligono simples contra uma janela convexa (um retangulo sempre e'), que
    e' exatamente o caso aqui (pais x bbox do painel, em graus)."""

    def clip_half_plane(points: list[tuple[float, float]], inside, intersect):
        if not points:
            return []
        out: list[tuple[float, float]] = []
        prev = points[-1]
        prev_in = inside(prev)
        for curr in points:
            curr_in = inside(curr)
            if curr_in:
                if not prev_in:
                    out.append(intersect(prev, curr))
                out.append(curr)
            elif prev_in:
                out.append(intersect(prev, curr))
            prev, prev_in = curr, curr_in
        return out

    def intersect_vertical(x: float):
        def fn(a, b):
            (ax, ay), (bx, by) = a, b
            t = 0.0 if bx == ax else (x - ax) / (bx - ax)
            return (x, ay + t * (by - ay))
        return fn

    def intersect_horizontal(y: float):
        def fn(a, b):
            (ax, ay), (bx, by) = a, b
            t = 0.0 if by == ay else (y - ay) / (by - ay)
            return (ax + t * (bx - ax), y)
        return fn

    pts = list(ring)
    pts = clip_half_plane(pts, lambda p: p[0] >= min_lon, intersect_vertical(min_lon))
    pts = clip_half_plane(pts, lambda p: p[0] <= max_lon, intersect_vertical(max_lon))
    pts = clip_half_plane(pts, lambda p: p[1] >= min_lat, intersect_horizontal(min_lat))
    pts = clip_half_plane(pts, lambda p: p[1] <= max_lat, intersect_horizontal(max_lat))
    return pts


def parse_sw_corner(base_name: str) -> Optional[tuple[int, int]]:
    """Extrai o canto SW (graus inteiros, sinal ja aplicado) do nome de um
    tile `*.hgt` -- mesma convencao de 11 caracteres, lida por POSICAO
    (nao ancorada no INICIO do nome: um prefixo qualquer antes do tag
    tambem casa, do jeito que `SrtmHgtFile::determineSrtmInfo()` casaria)
    que `app/src/app/TerrainQuery.cpp::parseSwCorner()` ja usa."""
    if len(base_name) < 11 or not base_name.endswith(".hgt"):
        return None
    tag = base_name[-11:-4]
    ns, ew = tag[0].upper(), tag[3].upper()
    lat_digits, lon_digits = tag[1:3], tag[4:7]
    if ns not in "NS" or ew not in "EW":
        return None
    if not (lat_digits.isdigit() and lon_digits.isdigit()):
        return None
    lat_deg = int(lat_digits)
    lon_deg = int(lon_digits)
    return (-lat_deg if ns == "S" else lat_deg, -lon_deg if ew == "W" else lon_deg)


def gzip_isize(path: Path) -> Optional[int]:
    """Tamanho do dado DESCOMPRIMIDO, lido do rodape do proprio `.gz`
    (RFC 1952 SS2.3.1: os ultimos 4 bytes do arquivo, little-endian) --
    sem descomprimir nada. `None` se o arquivo for curto demais para ter
    esse rodape (arquivo truncado/corrompido)."""
    try:
        size = path.stat().st_size
        if size < 4:
            return None
        with path.open("rb") as f:
            f.seek(-4, os.SEEK_END)
            return struct.unpack("<I", f.read(4))[0]
    except OSError:
        return None


def classify_size(size: Optional[int]) -> Status:
    if size == SRTM1_BYTES:
        return Status.SRTM1
    if size == SRTM3_BYTES:
        return Status.SRTM3
    return Status.UNKNOWN_SIZE


def scan_dir(root: Path) -> ScanResult:
    # Duas passadas: a primeira so' coleta, por celula, o que existe em disco
    # (nome, forma, tamanho); a segunda resolve o status de acordo com a
    # MESMA regra que `app::ensureTerrainData()` usa de verdade
    # (`app/src/app/TerrainData.cpp`) -- resolver por forma isolada, sem
    # olhar a outra, classificaria errado.
    raw: dict[tuple[int, int], list[tuple[str, Optional[int], str]]] = {}
    ignored: list[str] = []

    if not root.is_dir():
        return ScanResult(root=root, cells={}, ignored=ignored)

    for path in sorted(root.rglob("*")):
        if not path.is_file():
            continue
        rel = str(path.relative_to(root))
        name = path.name

        incomplete = name.endswith(".parcial")
        stem = name[: -len(".parcial")] if incomplete else name
        gz = stem.endswith(".gz")
        base = stem[: -len(".gz")] if gz else stem

        corner = parse_sw_corner(base)
        if corner is None:
            ignored.append(rel)
            continue

        if incomplete:
            form, size = "hgt.gz.parcial", None
        elif gz:
            form, size = "hgt.gz", gzip_isize(path)
        else:
            form, size = "hgt", path.stat().st_size

        raw.setdefault(corner, []).append((form, size, rel))

    cells: dict[tuple[int, int], CellCoverage] = {}
    for (sw_lat, sw_lon), entries in raw.items():
        cell = CellCoverage(sw_lat=sw_lat, sw_lon=sw_lon)
        cell.forms = {form for form, _, _ in entries}
        cell.paths = [rel for _, _, rel in entries]
        cell.hgt_size = next((size for form, size, _ in entries if form == "hgt"), None)
        cell.gz_isize = next((size for form, size, _ in entries if form == "hgt.gz"), None)

        if cell.hgt_size is not None and classify_size(cell.hgt_size) != Status.UNKNOWN_SIZE:
            # `ensureTerrainData()` nunca re-descomprime por cima de um '.hgt'
            # ja com tamanho SRTM valido, mesmo que o '.gz' ao lado tenha
            # mudado -- e' o '.hgt' que sera de fato lido pelo app, nao o
            # '.gz' (que so entra em jogo se o '.hgt' estiver AUSENTE ou com
            # tamanho invalido). Reportar o status do '.gz' aqui mascararia
            # um cache desatualizado real (medido acontecendo: 3 tiles deste
            # proprio repositorio tem um '.hgt' de resolucao SRTM3 parado no
            # disco ao lado de um '.hgt.gz' SRTM1 mais novo).
            cell.status = classify_size(cell.hgt_size)
            cell.size_bytes = cell.hgt_size
            if cell.gz_isize is not None and cell.gz_isize != cell.hgt_size:
                cell.stale_cache = True
        elif cell.gz_isize is not None:
            cell.status = classify_size(cell.gz_isize)
            cell.size_bytes = cell.gz_isize
        elif cell.hgt_size is not None:
            # '.hgt' presente mas com tamanho que nem SrtmHgtFile aceitaria --
            # o app tambem rejeitaria este arquivo (isValidSrtmSize()).
            cell.status = Status.UNKNOWN_SIZE
            cell.size_bytes = cell.hgt_size
        else:
            cell.status = Status.INCOMPLETE  # so' sobrou '.hgt.gz.parcial'

        cells[(sw_lat, sw_lon)] = cell

    return ScanResult(root=root, cells=cells, ignored=ignored)


def cluster_cells(cells: dict[tuple[int, int], CellCoverage]) -> list[Cluster]:
    """Agrupa celulas 8-conectadas (compartilham lado OU quina) em regioes
    contiguas -- a nocao grafica de "uma area do globo", nao uma celula
    isolada. BFS simples: a quantidade de tiles em jogo aqui (dezenas a
    poucos milhares, mesmo com toda a cobertura de `--brasil`) dispensa
    qualquer estrutura mais esperta."""
    remaining = set(cells.keys())
    clusters: list[Cluster] = []
    while remaining:
        seed = remaining.pop()
        group = [seed]
        frontier = [seed]
        while frontier:
            lat, lon = frontier.pop()
            for dlat in (-1, 0, 1):
                for dlon in (-1, 0, 1):
                    if dlat == 0 and dlon == 0:
                        continue
                    neighbor = (lat + dlat, lon + dlon)
                    if neighbor in remaining:
                        remaining.remove(neighbor)
                        group.append(neighbor)
                        frontier.append(neighbor)
        clusters.append(Cluster(cells=group))
    clusters.sort(key=lambda c: len(c.cells), reverse=True)
    return clusters


def approx_area_km2(cells: dict[tuple[int, int], CellCoverage]) -> float:
    """Soma, celula a celula, a area aproximada de 1 grau x 1 grau naquela
    latitude (o meridiano encolhe com cos(lat) -- longe de exato, mas
    suficiente para uma ordem de grandeza no resumo)."""
    total = 0.0
    for sw_lat, _ in cells:
        lat_center = sw_lat + 0.5
        km_per_deg_lon = KM_PER_DEG_LAT * math.cos(math.radians(lat_center))
        total += KM_PER_DEG_LAT * km_per_deg_lon
    return total


def format_lat(deg: int) -> str:
    if deg == 0:
        return "0"
    return f"{abs(deg)}{'S' if deg < 0 else 'N'}"


def format_lon(deg: int) -> str:
    if deg == 0:
        return "0"
    return f"{abs(deg)}{'W' if deg < 0 else 'E'}"


def wrap_text_lines(text: str, width_px: float, size: int) -> list[str]:
    """Quebra de linha por CONTAGEM de caracteres, nao por medida real de
    glifo -- `textwrap` e' exato o bastante pro uso aqui (um banner de aviso
    em fonte sans-serif) com uma largura media de caractere aproximada
    (~0.56 * tamanho da fonte, medida boa o suficiente para nao estourar a
    largura do banner na pratica; nunca precisa ser pixel-perfeito, so' nao
    cortar texto)."""
    avg_char_px = size * 0.56
    max_chars = max(20, int(width_px / avg_char_px))
    return textwrap.wrap(text, width=max_chars) or [text]


def svg_escape(text: str) -> str:
    return (
        text.replace("&", "&amp;")
        .replace("<", "&lt;")
        .replace(">", "&gt;")
        .replace('"', "&quot;")
    )


class SvgBuilder:
    """Acumulador de elementos SVG -- so' para nao repetir a mesma
    formatacao de f-string dezenas de vezes abaixo."""

    def __init__(self) -> None:
        self.parts: list[str] = []

    def raw(self, s: str) -> None:
        self.parts.append(s)

    def rect(self, x: float, y: float, w: float, h: float, fill: str, stroke: str = "none",
              stroke_width: float = 0.0, opacity: float = 1.0, title: Optional[str] = None,
              rx: float = 0.0) -> None:
        title_tag = f"<title>{svg_escape(title)}</title>" if title else ""
        self.parts.append(
            f'<rect x="{x:.2f}" y="{y:.2f}" width="{w:.2f}" height="{h:.2f}" '
            f'fill="{fill}" stroke="{stroke}" stroke-width="{stroke_width}" '
            f'fill-opacity="{opacity}" rx="{rx}">{title_tag}</rect>'
        )

    def line(self, x1: float, y1: float, x2: float, y2: float, stroke: str,
              stroke_width: float = 1.0, dash: Optional[str] = None) -> None:
        dash_attr = f' stroke-dasharray="{dash}"' if dash else ""
        self.parts.append(
            f'<line x1="{x1:.2f}" y1="{y1:.2f}" x2="{x2:.2f}" y2="{y2:.2f}" '
            f'stroke="{stroke}" stroke-width="{stroke_width}"{dash_attr}/>'
        )

    def text(self, x: float, y: float, s: str, size: int = 12, fill: str = "#222",
              anchor: str = "start", weight: str = "normal", family: str = "monospace") -> None:
        self.parts.append(
            f'<text x="{x:.2f}" y="{y:.2f}" font-family="{family}" font-size="{size}" '
            f'fill="{fill}" text-anchor="{anchor}" font-weight="{weight}">{svg_escape(s)}</text>'
        )

    def circle(self, cx: float, cy: float, r: float, fill: str, stroke: str = "none") -> None:
        self.parts.append(f'<circle cx="{cx:.2f}" cy="{cy:.2f}" r="{r:.2f}" fill="{fill}" stroke="{stroke}"/>')

    def polygon(self, points: list[tuple[float, float]], fill: str, stroke: str = "none",
                 stroke_width: float = 0.0, opacity: float = 1.0) -> None:
        if len(points) < 3:
            return
        pts = " ".join(f"{x:.1f},{y:.1f}" for x, y in points)
        self.parts.append(
            f'<polygon points="{pts}" fill="{fill}" stroke="{stroke}" '
            f'stroke-width="{stroke_width}" fill-opacity="{opacity}"/>'
        )

    def render(self) -> str:
        return "\n".join(self.parts)


# Paleta cartografica -- terra/oceano/borda de pais, compartilhada pelos dois
# paineis (mapa-mundi e detalhe). O contorno real vem de
# tools/data/world_countries_110m.json (ver load_countries()); sem ele os dois
# paineis caem de volta pro oceano liso (nenhum crash, so' menos contexto).
LAND_FILL = "#f1ede1"
LAND_STROKE = "#b8ac93"
OCEAN_FILL = "#d9e6f2"
OCEAN_STROKE = "#9fb4c7"
GRID_STROKE = "#b7c3cd"
GRID_STRONG = "#8aa0ae"

MARGIN = 50
GAP = 24
CONTENT_W = 1000                       # largura de conteudo entre as margens
WORLD_W = 680
WORLD_H = round(WORLD_W / 2)           # equirretangular, 360x180 graus
LEGEND_W = CONTENT_W - WORLD_W - GAP
DETAIL_MAX_W = CONTENT_W
DETAIL_MAX_H = 620
DETAIL_MIN_CELL_PX = 10
DETAIL_MAX_CELL_PX = 46
PANEL_GAP = 40
MAX_DETAIL_PANELS = 6


def render_world_panel(clusters: list[Cluster], top: float) -> tuple[SvgBuilder, float]:
    svg = SvgBuilder()
    cell_px = WORLD_W / 360.0

    def proj(lat: float, lon: float) -> tuple[float, float]:
        return (MARGIN + (lon + 180.0) * cell_px, top + (90.0 - lat) * cell_px)

    x0, y0 = proj(90, -180)
    svg.rect(x0, y0, WORLD_W, WORLD_H, fill=OCEAN_FILL, stroke=OCEAN_STROKE, stroke_width=1.0, rx=6)

    for country in load_countries():
        for ring in country.rings:
            svg.polygon([proj(lat, lon) for lon, lat in ring], fill=LAND_FILL,
                         stroke=LAND_STROKE, stroke_width=0.5)

    # graticula a cada 30 graus, com equador/tropicos em destaque tracejado
    for lat in range(-90, 91, 30):
        y = proj(lat, 0)[1]
        svg.line(x0, y, x0 + WORLD_W, y, stroke=GRID_STROKE, stroke_width=0.7)
        svg.text(x0 - 8, y + 4, format_lat(lat), size=10, fill="#5b6b78", anchor="end")
    for lon in range(-180, 181, 30):
        x = proj(0, lon)[0]
        svg.line(x, y0, x, y0 + WORLD_H, stroke=GRID_STROKE, stroke_width=0.7)
        svg.text(x, y0 + WORLD_H + 14, format_lon(lon), size=10, fill="#5b6b78", anchor="middle")
    for lat in (0, 23, -23):
        y = proj(lat, 0)[1]
        svg.line(x0, y, x0 + WORLD_W, y, stroke=GRID_STRONG, stroke_width=1.0, dash="4,3")

    for idx, cluster in enumerate(clusters):
        cx0, cy0 = proj(cluster.max_lat + 1, cluster.min_lon)
        cx1, cy1 = proj(cluster.min_lat, cluster.max_lon + 1)
        w = max(cx1 - cx0, 3.0)
        h = max(cy1 - cy0, 3.0)
        svg.rect(cx0 - 1.5, cy0 - 1.5, w + 3, h + 3, fill="#d1495b", stroke="#7a1f2b",
                  stroke_width=1.3, opacity=0.88,
                  title=f"regiao {idx + 1}: {len(cluster.cells)} tile(s)")

    svg.text(x0, top - 12, "Visao global", size=14, weight="bold", fill="#1f2d3a", family="sans-serif")
    return svg, top + WORLD_H


def render_detail_panel(cluster: Cluster, cells: dict[tuple[int, int], CellCoverage],
                          top: float, index: int) -> tuple[SvgBuilder, float]:
    svg = SvgBuilder()
    min_lat, max_lat = cluster.min_lat - 1, cluster.max_lat + 2  # +1 grau de folga visual
    min_lon, max_lon = cluster.min_lon - 1, cluster.max_lon + 2
    width_deg = max_lon - min_lon
    height_deg = max_lat - min_lat

    cell_px = min(DETAIL_MAX_W / width_deg, DETAIL_MAX_H / height_deg)
    cell_px = max(DETAIL_MIN_CELL_PX, min(DETAIL_MAX_CELL_PX, cell_px))
    panel_w = width_deg * cell_px
    panel_h = height_deg * cell_px

    def proj(lat: float, lon: float) -> tuple[float, float]:
        return (MARGIN + (lon - min_lon) * cell_px, top + (max_lat - lat) * cell_px)

    x0, y0 = proj(max_lat, min_lon)
    svg.rect(x0, y0, panel_w, panel_h, fill=OCEAN_FILL, stroke=OCEAN_STROKE, stroke_width=1.0, rx=6)

    panel_bbox = (min_lon, min_lat, max_lon, max_lat)
    for country in load_countries():
        if not bbox_overlaps(country.bbox, panel_bbox):
            continue
        for ring in country.rings:
            clipped = clip_ring_to_bbox(ring, min_lon, min_lat, max_lon, max_lat)
            if len(clipped) < 3:
                continue
            svg.polygon([proj(lat, lon) for lon, lat in clipped], fill=LAND_FILL,
                         stroke=LAND_STROKE, stroke_width=0.6)

    label_step = 1 if cell_px >= 22 else max(1, round(5 / max(cell_px, 1)) * 5)
    for lat in range(math.ceil(min_lat), math.floor(max_lat) + 1):
        y = proj(lat, min_lon)[1]
        svg.line(x0, y, x0 + panel_w, y, stroke=GRID_STROKE, stroke_width=0.5)
        if lat % label_step == 0:
            svg.text(x0 - 6, y + 4, format_lat(lat), size=10, fill="#5b6b78", anchor="end")
    for lon in range(math.ceil(min_lon), math.floor(max_lon) + 1):
        x = proj(max_lat, lon)[0]
        svg.line(x, y0, x, y0 + panel_h, stroke=GRID_STROKE, stroke_width=0.5)
        if lon % label_step == 0:
            svg.text(x, y0 + panel_h + 14, format_lon(lon), size=10, fill="#5b6b78", anchor="middle")

    for sw_lat, sw_lon in cluster.cells:
        info = cells[(sw_lat, sw_lon)]
        cx, cy = proj(sw_lat + 1, sw_lon)
        forms = "+".join(sorted(info.forms))
        size_note = f", {info.size_bytes} bytes" if info.size_bytes is not None else ""
        title = f"{info.name} -- {info.status.value} [{forms}]{size_note}"
        if info.stale_cache:
            title += (f" -- ATENCAO: .hgt em disco ({info.hgt_size} bytes) diverge do "
                      f".hgt.gz ({info.gz_isize} bytes); o app serve o .hgt ate ele ser apagado")
        svg.rect(cx, cy, cell_px, cell_px, fill=STATUS_COLOR[info.status],
                  stroke="#2b2b2b", stroke_width=0.4, opacity=0.92, title=title)
        if info.stale_cache:
            # cache desatualizado: marca com um X preto por cima da celula --
            # a cor de status continua sendo a do que sera SERVIDO (o '.hgt'),
            # o X e' so' o alerta de que o '.hgt.gz' ao lado ja diverge dele.
            pad = cell_px * 0.22
            svg.line(cx + pad, cy + pad, cx + cell_px - pad, cy + cell_px - pad,
                      stroke="#000000", stroke_width=max(1.2, cell_px * 0.06))
            svg.line(cx + cell_px - pad, cy + pad, cx + pad, cy + cell_px - pad,
                      stroke="#000000", stroke_width=max(1.2, cell_px * 0.06))

    breakdown = ", ".join(
        f"{n} {status.name}" for status in STATUS_PRIORITY
        if (n := sum(1 for c in cluster.cells if cells[c].status == status))
    )
    bbox_label = (
        f"Regiao {index + 1} -- {len(cluster.cells)} tile(s) ({breakdown}) -- "
        f"lat {format_lat(cluster.min_lat)}..{format_lat(cluster.max_lat)}, "
        f"lon {format_lon(cluster.min_lon)}..{format_lon(cluster.max_lon)}"
    )
    svg.text(x0, top - 10, bbox_label, size=13, weight="bold", fill="#1f2d3a", family="sans-serif")
    return svg, top + panel_h + PANEL_GAP + 20


def render_stat_tiles(top: float, tiles: list[tuple[str, str, bool]]) -> tuple[SvgBuilder, float]:
    """Faixa de cartoes pequenos (valor + rotulo) -- o resumo numerico que
    antes só existia no texto corrido do subtitulo, agora escaneável num
    relance, mesmo idioma visual de um HUD/dashboard."""
    svg = SvgBuilder()
    n = len(tiles)
    gap = 14
    w = (CONTENT_W - gap * (n - 1)) / n
    h = 62
    for i, (value, label, warn) in enumerate(tiles):
        x = MARGIN + i * (w + gap)
        bg, border, value_color = ("#fdeee9", "#c94f3d", "#9c3b28") if warn else ("#f6f8f7", "#d7e0da", "#12202c")
        svg.rect(x, top, w, h, fill=bg, stroke=border, stroke_width=1.0, rx=10)
        svg.text(x + 14, top + 30, value, size=19, weight="bold", fill=value_color, family="sans-serif")
        svg.text(x + 14, top + 48, label, size=10, fill="#5b6b78", family="sans-serif")
    return svg, top + h


def render_alert_banner(top: float, lines: list[str]) -> tuple[SvgBuilder, float]:
    svg = SvgBuilder()
    pad = 14
    line_h = 16
    h = pad * 2 + 20 + len(lines) * line_h
    svg.rect(MARGIN, top, CONTENT_W, h, fill="#fdeee9", stroke="#c94f3d", stroke_width=1.0, rx=8)
    svg.text(MARGIN + pad, top + pad + 8, "⚠ ATENCAO -- cache de terreno desatualizado",
              size=12, weight="bold", fill="#9c3b28", family="sans-serif")
    y = top + pad + 8 + 20
    for line in lines:
        svg.text(MARGIN + pad, y, line, size=11, fill="#5c2a20", family="sans-serif")
        y += line_h
    return svg, top + h


def render_legend_panel(top: float) -> tuple[SvgBuilder, float]:
    """Card ao lado do mapa-mundi (nao mais uma tira solta no rodape) -- as
    mesmas entradas de sempre, mais terra/oceano agora que o contorno de
    pais existe."""
    svg = SvgBuilder()
    x = MARGIN + WORLD_W + GAP
    pad = 14
    row_h = 22
    n_rows = len(STATUS_PRIORITY) + 3  # + regiao contigua, cache desatualizado, terra
    content_h = pad * 2 + n_rows * row_h
    svg.rect(x, top, LEGEND_W, content_h, fill="#fbfbfa", stroke="#d7ded8", stroke_width=1.0, rx=8)
    svg.text(x + pad, top - 12, "Legenda", size=14, weight="bold", fill="#1f2d3a", family="sans-serif")

    y = top + pad + 12
    for status in STATUS_PRIORITY:
        svg.rect(x + pad, y - 12, 16, 16, fill=STATUS_COLOR[status], stroke="#2b2b2b",
                  stroke_width=0.4, rx=3)
        svg.text(x + pad + 24, y + 1, status.value, size=11, fill="#333", family="sans-serif")
        y += row_h

    svg.rect(x + pad, y - 12, 16, 16, fill="#d1495b", stroke="#7a1f2b", stroke_width=0.6,
              opacity=0.85, rx=3)
    svg.text(x + pad + 24, y + 1, "regiao contigua (mapa global)", size=11, fill="#333",
              family="sans-serif")
    y += row_h

    svg.rect(x + pad, y - 12, 16, 16, fill="#9e9e9e", stroke="#2b2b2b", stroke_width=0.4, rx=3)
    svg.line(x + pad + 3, y - 9, x + pad + 13, y + 1, stroke="#000000", stroke_width=1.2)
    svg.line(x + pad + 13, y - 9, x + pad + 3, y + 1, stroke="#000000", stroke_width=1.2)
    svg.text(x + pad + 24, y + 1, "cache .hgt desatualizado", size=11, fill="#333", family="sans-serif")
    y += row_h

    svg.rect(x + pad, y - 12, 16, 16, fill=LAND_FILL, stroke=LAND_STROKE, stroke_width=0.8, rx=3)
    svg.text(x + pad + 24, y + 1, "terra (contorno de pais)", size=11, fill="#333", family="sans-serif")

    return svg, top + content_h


def build_svg(scan: ScanResult, clusters: list[Cluster], generated_at: str) -> str:
    """Disposicao, de cima pra baixo: cabecalho -> faixa de cartoes com os
    numeros principais -> banner de alerta (so' se houver cache
    desatualizado) -> mapa-mundi + legenda LADO A LADO (a legenda vale para
    os dois paineis, mundial e de detalhe) -> um painel de detalhe por
    regiao, largura cheia -> rodape com o que foi ignorado."""
    counts = {status: 0 for status in STATUS_PRIORITY}
    for c in scan.cells.values():
        counts[c.status] += 1
    stale = [c for c in scan.cells.values() if c.stale_cache]

    header = SvgBuilder()
    header.text(MARGIN, 32, "Cobertura de elevacao SRTM", size=22, weight="bold",
                 fill="#12202c", family="sans-serif")
    header.text(MARGIN, 52, str(scan.root), size=12, fill="#5b6b78", family="monospace")
    header.text(MARGIN + CONTENT_W, 32, f"gerado em {generated_at}", size=10, fill="#8a97a1",
                 anchor="end", family="monospace")
    top = 52 + 28

    tiles = [
        (str(len(scan.cells)), "celulas de 1x1 grau", False),
        (f"~{approx_area_km2(scan.cells):,.0f} km2".replace(",", "."), "area aproximada", False),
        (str(len(clusters)), "regiao(oes) contigua(s)", False),
        (f"{counts[Status.SRTM1]} / {counts[Status.SRTM3]}", "SRTM1 / SRTM3", False),
    ]
    if stale:
        tiles.append((str(len(stale)), "cache desatualizado", True))
    tiles_svg, top = render_stat_tiles(top, tiles)
    top += 30

    alert_svg = None
    if stale:
        names = ", ".join(sorted(c.name for c in stale))
        msg = (
            f"{len(stale)} celula(s) com '.hgt' em disco divergindo do '.hgt.gz' ao lado -- o app "
            f"so' re-descomprime se o '.hgt' estiver ausente ou com tamanho invalido, entao serve a "
            f"resolucao ERRADA ate o arquivo ser apagado a mao: {names}"
        )
        lines = wrap_text_lines(msg, CONTENT_W - 28, 11)
        alert_svg, top = render_alert_banner(top, lines)
        top += 26

    top += 16  # espaco pro titulo "Visao global" / "Legenda"
    world_svg, world_bottom = render_world_panel(clusters, top)
    legend_svg, legend_bottom = render_legend_panel(top)
    top = max(world_bottom, legend_bottom) + 40

    detail_blocks: list[SvgBuilder] = []
    shown = clusters[:MAX_DETAIL_PANELS]
    for idx, cluster in enumerate(shown):
        block, top = render_detail_panel(cluster, scan.cells, top, idx)
        detail_blocks.append(block)
    if len(clusters) > MAX_DETAIL_PANELS:
        extra = len(clusters) - MAX_DETAIL_PANELS
        note = SvgBuilder()
        note.text(MARGIN, top, f"(+{extra} regiao(oes) menor(es) fora do detalhe -- ver coverage.json)",
                    size=11, fill="#7a1f2b", family="sans-serif")
        detail_blocks.append(note)
        top += 24

    if scan.ignored:
        preview = ", ".join(scan.ignored[:8])
        more = f" (+{len(scan.ignored) - 8})" if len(scan.ignored) > 8 else ""
        ignored_note = SvgBuilder()
        ignored_note.text(MARGIN, top, f"Ignorados (fora da convencao SRTM): {preview}{more}",
                            size=10, fill="#7a1f2b", family="sans-serif")
        detail_blocks.append(ignored_note)
        top += 20

    total_h = top + MARGIN
    total_w = CONTENT_W + 2 * MARGIN

    doc = SvgBuilder()
    doc.raw(f'<svg xmlns="http://www.w3.org/2000/svg" width="{total_w}" height="{total_h}" '
             f'viewBox="0 0 {total_w} {total_h}" font-family="sans-serif">')
    doc.rect(0, 0, total_w, total_h, fill="#ffffff")
    doc.raw(header.render())
    doc.raw(tiles_svg.render())
    if alert_svg is not None:
        doc.raw(alert_svg.render())
    doc.raw(world_svg.render())
    doc.raw(legend_svg.render())
    for block in detail_blocks:
        doc.raw(block.render())
    doc.raw("</svg>")
    return doc.render()


def build_json_manifest(scan: ScanResult, clusters: list[Cluster], generated_at: str) -> dict:
    return {
        "generated_at": generated_at,
        "source_dir": str(scan.root),
        "cell_count": len(scan.cells),
        "approx_area_km2": round(approx_area_km2(scan.cells), 1),
        "ignored_files": scan.ignored,
        "clusters": [
            {
                "tile_count": len(cluster.cells),
                "bbox": {
                    "min_lat": cluster.min_lat,
                    "max_lat": cluster.max_lat + 1,
                    "min_lon": cluster.min_lon,
                    "max_lon": cluster.max_lon + 1,
                },
                "cells": [
                    {
                        "name": scan.cells[cell].name,
                        "sw_lat": cell[0],
                        "sw_lon": cell[1],
                        "status": scan.cells[cell].status.name,
                        "size_bytes": scan.cells[cell].size_bytes,
                        "forms": sorted(scan.cells[cell].forms),
                        "stale_cache": scan.cells[cell].stale_cache,
                        "hgt_size": scan.cells[cell].hgt_size,
                        "gz_isize": scan.cells[cell].gz_isize,
                    }
                    for cell in sorted(cluster.cells)
                ],
            }
            for cluster in clusters
        ],
    }


def print_summary(scan: ScanResult, clusters: list[Cluster]) -> None:
    total = len(scan.cells)
    if total == 0:
        print(f"Nenhum tile SRTM reconhecido sob {scan.root}", file=sys.stderr)
        if scan.ignored:
            print(f"  ({len(scan.ignored)} arquivo(s) ignorado(s), fora da convencao)", file=sys.stderr)
        return

    counts: dict[Status, int] = {status: 0 for status in STATUS_PRIORITY}
    for cell in scan.cells.values():
        counts[cell.status] += 1

    print(f"Diretorio: {scan.root}")
    print(f"Celulas de 1x1 grau com cobertura: {total}")
    print(f"Area aproximada: {approx_area_km2(scan.cells):,.0f} km2".replace(",", "."))
    print(f"Regioes contiguas (8-conexas): {len(clusters)}")
    for status in STATUS_PRIORITY:
        if counts[status]:
            print(f"  {status.value}: {counts[status]}")
    for idx, cluster in enumerate(clusters):
        print(
            f"  regiao {idx + 1}: {len(cluster.cells)} tile(s), "
            f"lat {format_lat(cluster.min_lat)}..{format_lat(cluster.max_lat)}, "
            f"lon {format_lon(cluster.min_lon)}..{format_lon(cluster.max_lon)}"
        )

    stale = [c for c in scan.cells.values() if c.stale_cache]
    if stale:
        names = ", ".join(sorted(c.name for c in stale))
        print(
            f"ATENCAO: {len(stale)} celula(s) com cache .hgt desatualizado -- o '.hgt' em "
            f"disco diverge do '.hgt.gz' ao lado e continuara sendo servido do jeito ERRADO "
            f"ate ser apagado (ensureTerrainData() so' descomprime de novo se o '.hgt' estiver "
            f"ausente ou com tamanho invalido): {names}"
        )

    if scan.ignored:
        print(f"Ignorados (fora da convencao SRTM): {len(scan.ignored)} -- {', '.join(scan.ignored[:5])}"
              + (" ..." if len(scan.ignored) > 5 else ""))


def main(argv: Optional[list[str]] = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dir", default=str(DEFAULT_DIR),
                     help=f"pasta a varrer, recursivamente (default: {DEFAULT_DIR})")
    ap.add_argument("--out", default=None,
                     help=f"caminho do .svg de saida (default: {DEFAULT_OUT_DIR / 'coverage.svg'})")
    ap.add_argument("--json", dest="json_out", default=None,
                     help="caminho do .json de saida (default: ao lado do --out); "
                     "'-' desliga a escrita do manifesto")
    ap.add_argument("--open", action="store_true",
                     help="abre o SVG gerado no navegador (via scripts/open_browser.sh)")
    ap.add_argument("--quiet", action="store_true", help="nao imprime o resumo textual")
    args = ap.parse_args(argv)

    root = Path(args.dir).expanduser()
    if not root.is_absolute():
        root = (REPO_ROOT / root).resolve()

    out_svg = Path(args.out).expanduser().resolve() if args.out else DEFAULT_OUT_DIR / "coverage.svg"
    if args.json_out == "-":
        out_json: Optional[Path] = None
    elif args.json_out:
        out_json = Path(args.json_out).expanduser().resolve()
    else:
        out_json = out_svg.with_suffix(".json")

    scan = scan_dir(root)
    clusters = cluster_cells(scan.cells)
    generated_at = datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M:%SZ")

    out_svg.parent.mkdir(parents=True, exist_ok=True)
    out_svg.write_text(build_svg(scan, clusters, generated_at), encoding="utf-8")

    if out_json is not None:
        out_json.parent.mkdir(parents=True, exist_ok=True)
        out_json.write_text(
            json.dumps(build_json_manifest(scan, clusters, generated_at), indent=2, ensure_ascii=False) + "\n",
            encoding="utf-8",
        )

    if not args.quiet:
        print_summary(scan, clusters)
        print(f"\nSVG: {out_svg}")
        if out_json is not None:
            print(f"JSON: {out_json}")

    if args.open:
        opener = REPO_ROOT / "scripts" / "open_browser.sh"
        if opener.is_file():
            subprocess.run([str(opener), str(out_svg)], check=False)
        else:
            print(f"aviso: {opener} nao encontrado, abra {out_svg} manualmente", file=sys.stderr)

    return 0 if scan.cells else 1


if __name__ == "__main__":
    sys.exit(main())
