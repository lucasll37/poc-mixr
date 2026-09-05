#!/usr/bin/env python3
"""Gera os dados JS do modo "Catálogo mixr::models" de docs/explorador/index.html,
a partir de scripts/extract_execution_chain.py --catalog.

Isto é um passo de GERAÇÃO (roda uma vez, a mão, sempre que contexts/src/mixr
mudar) -- o index.html final continua um arquivo estático autossuficiente, sem
build nem servidor para quem só quer ABRIR a página. Ver docs/explorador/
README.md, seção "Catálogo mixr::models", para o porquê da separação.

Produz três coisas, em JS pronto para colar num <script> do index.html:
  - CATALOG_SRC   -- um SRC[] a mais, com o corpo REAL (capturado, nao
                     transcrito a mao) de cada metodo que alguma das ~120
                     classes de mixr::models sobrescreve.
  - TAXONOMY      -- a arvore de heranca de VERDADE (Object no topo), unica
                     estrutura logica que faz sentido para "todos os modelos"
                     ao mesmo tempo -- ao contrario de uma arvore de instancia
                     de UM cenario, aqui nao ha instancia nenhuma, so classes.
  - CATALOG_TOUR  -- um passo por override encontrado, na ordem da propria
                     arvore (pre-ordem), para dar a sensacao de "passo a
                     passo" que uma tela cheia de cards nao daria sozinha.

Uso:
    python3 scripts/generate_catalog_js.py > /tmp/catalog_data.js
    # depois: colar o conteudo dentro de um <script> em docs/explorador/index.html
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import extract_execution_chain as ext  # noqa: E402


def js_line(s: str) -> str:
    """Uma linha de texto real, como template literal JS seguro (escapa \\,
    ` e ${ -- os tres jeitos de um backtick-string quebrar)."""
    s = s.replace("\\", "\\\\").replace("`", "\\`").replace("${", "\\${")
    return f"`{s}`"


def js_str(s) -> str:
    return json.dumps(s, ensure_ascii=False)


def build():
    inheritance = ext.build_inheritance([ext.MIXR_INCLUDE])
    overrides = ext.find_overrides([ext.MIXR_SRC], capture_body=True)
    catalog = ext.build_catalog()  # já usa inheritance/overrides restritos como precisa

    by_class = {e["class"]: e for e in catalog}
    catalog_names = set(by_class)

    # --- 1) universo de nos da arvore: as ~120 classes do catalogo + toda
    # classe-ponte no caminho ate Object (Component, System, Player, ...) ---
    node_names = set(catalog_names)
    for cls in list(catalog_names):
        node_names.update(ext.resolve_chain(cls, inheritance))
    node_names.add("Object")

    # --- 2) monta pai->filhos a partir de 'inheritance' (Tipo -> BaseType) ---
    children_of: dict[str, list[str]] = {}
    for cls in sorted(node_names):
        base = inheritance.get(cls)
        if base and base in node_names:
            children_of.setdefault(base, []).append(cls)

    # --- 3) fabrica -> classe (para saber o nome de fabrica de cada no) ----
    factory_map = ext.build_factory_map([ext.MIXR_MODELS_SRC])
    class_to_factory = {}
    for factory_name, cls in factory_map.items():
        class_to_factory.setdefault(cls, factory_name)

    src_entries: dict[str, dict] = {}

    def src_key(cls, meth):
        return f"{cls}::{meth}"

    def node_info(cls):
        chain = ext.resolve_chain(cls, inheritance)
        any_work = False
        for level in chain:
            if level in ext.GENERIC_DISPATCH_CLASSES:
                continue
            lv = overrides.get(level, {})
            if any(v["nonempty"] for v in lv.values()):
                any_work = True
        own = overrides.get(cls, {})
        own_keys = []
        for meth, info in own.items():
            if not info["nonempty"]:
                continue
            key = src_key(cls, meth)
            blocks = [{"start": info["startLine"], "lines": info["lines"]}]
            if info.get("truncated"):
                blocks.append({"omitted": f"corpo continua além da linha {info['startLine'] + len(info['lines']) - 1} (capturado automaticamente, corte por tamanho)"})
            src_entries[key] = {"file": info["file"], "blocks": blocks}
            own_keys.append(key)
        entry = by_class.get(cls)
        return {
            "id": cls,
            "cls": cls,
            "factory": class_to_factory.get(cls),
            "category": entry["category"] if entry else "framework",
            "bridge": entry is None,
            "chain": chain,
            "dispatch": next((lvl for lvl in chain if lvl in ("System", "Component", "Simulation")), None),
            "idle": not any_work,
            "srcKeys": own_keys,
            "slots": entry["slots"] if entry else [],
        }

    def build_node(cls):
        info = node_info(cls)
        kids = sorted(children_of.get(cls, []))
        info["children"] = [build_node(k) for k in kids]
        return info

    taxonomy = build_node("Object")

    # --- 4) tour: pre-ordem, um passo por metodo sobrescrito real ----------
    tour = []

    def walk(node, path):
        my_path = path + [node["id"]]
        if not node["bridge"]:
            for key in node["srcKeys"]:
                meth = key.split("::")[1]
                entry = src_entries[key]
                real_block = next((b for b in entry["blocks"] if "start" in b), None)
                hl = [real_block["start"], real_block["start"] + len(real_block["lines"]) - 1] if real_block else None
                tour.append({
                    "node": node["id"],
                    "src": key,
                    "hl": hl,
                    "idle": False,
                    "runs": True,
                    "title": f"{key}()",
                    "body": f"Classe {node['id']} (categoria {node['category']}, fábrica "
                            f"{node['factory'] or '—'}) sobrescreve {meth}(). Cadeia: "
                            + " → ".join(node["chain"]) + ".",
                    "stack": [{"label": n, "node": n} for n in my_path],
                })
            if not node["srcKeys"]:
                tour.append({
                    "node": node["id"],
                    "src": None,
                    "hl": None,
                    "idle": True,
                    "runs": False,
                    "title": f"{node['id']} — ocioso pelos 7 métodos",
                    "body": f"Classe {node['id']} (categoria {node['category']}, fábrica "
                            f"{node['factory'] or '—'}) não sobrescreve nenhum dos 7 métodos "
                            "de fase em nenhum nível não-genérico da cadeia. "
                            + (f"Interface EDL: {len(node['slots'])} slot(s)." if node["slots"] else "Sem slots próprios encontrados."),
                    "stack": [{"label": n, "node": n} for n in my_path],
                })
        for child in node["children"]:
            walk(child, my_path)

    walk(taxonomy, [])

    return src_entries, taxonomy, tour


def emit_src(src_entries):
    out = ["const CATALOG_SRC = {"]
    for key in sorted(src_entries):
        e = src_entries[key]
        out.append(f"{js_str(key)}: {{")
        out.append(f"  file: {js_str(e['file'])},")
        out.append("  blocks: [")
        for b in e["blocks"]:
            if "omitted" in b:
                out.append(f"    {{ omitted: {js_str(b['omitted'])} }},")
            else:
                out.append(f"    {{ start: {b['start']}, lines: [")
                for line in b["lines"]:
                    out.append(f"      {js_line(line)},")
                out.append("    ] },")
        out.append("  ],")
        out.append("},")
    out.append("};")
    return "\n".join(out)


def emit_json_const(name, value):
    return f"const {name} = {json.dumps(value, indent=2, ensure_ascii=False)};"


def main():
    src_entries, taxonomy, tour = build()
    print("/* gerado por scripts/generate_catalog_js.py -- nao editar a mao */")
    print(emit_src(src_entries))
    print()
    print(emit_json_const("TAXONOMY", taxonomy))
    print()
    print(emit_json_const("CATALOG_TOUR", tour))
    print()
    print(f"// {len(src_entries)} trechos de fonte, {sum(1 for _ in _walk_count(taxonomy))} nos na taxonomia, {len(tour)} passos no tour", file=sys.stderr)


def _walk_count(node):
    yield node
    for c in node["children"]:
        yield from _walk_count(c)


if __name__ == "__main__":
    main()
