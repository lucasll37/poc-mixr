import React, { useState, useMemo, useRef, useEffect, useCallback } from "react";

/* ==================================================================== *
 * MIXR — Editor gráfico de cenário .edl
 *
 * EDL_CATALOG é GERADO por `make edl-catalog` (scripts/extract_execution_
 * chain.py --edl-catalog) e injetado num <script> próprio por docs/
 * compile.js, ANTES deste arquivo -- ver o comentário de `dataFile` em
 * compile.js. Cobre TODAS as factories que app/src/mixr_factory.cpp de
 * fato encadeia (base/models/terrain/interop-dis/linkage/recorder/
 * simulation), mais shared/x* e os plugins deste repositório -- não é um
 * recorte curado.
 *
 * A lógica PURA (índice do catálogo, compatibilidade de slot, o modelo de
 * dados do projeto e o serializador para texto .edl) mora em
 * src/ui/edl_builder_core.js, não aqui -- compile.js concatena os dois no
 * MESMO <script>, então as funções de lá (buildCatalogIndex, isCompatible,
 * makeNode, projectToEdl, ...) já chegam em escopo, sem `import`. Ver o
 * cabeçalho daquele arquivo para o porquê (é o que permite testá-la em
 * Node puro, sem Babel/React/DOM, em src/ui/edl_builder.test.js).
 *
 * Escopo desta v1 (decidido explicitamente, não suposto): CRIAR um
 * cenário do zero -- arrastar classes da paleta, preencher campos,
 * exportar .edl. Reabrir um .edl REAL existente fica para uma fase
 * futura -- o formato de projeto (JSON) desta página é PRÓPRIO, não é o
 * EDL em si, e serve só para salvar/reabrir um trabalho em andamento
 * nesta ferramenta.
 * ==================================================================== */

const CATALOG_BY_FACTORY = buildCatalogIndex(EDL_CATALOG);

// A raiz do cenário TEM que ser algo que implementa Station -- nunca
// "qualquer classe" (era assim antes: objectTypes:["Object"], que qualquer
// coisa satisfaz). isCompatible() já sabe aceitar subclasse (ClockStation
// deriva de Station), então isto sozinho já cobre as duas raízes reais
// deste repositório sem precisar listar as duas à mão.
const ROOT_SLOT_DEF = { objectTypes: ["Station"], acceptsChildList: false };

/* --------------------------------- widgets -------------------------------- */

function TextField({ value, onChange, placeholder }) {
  return (
    <input className="eb-input" type="text" value={value} placeholder={placeholder}
      onChange={(e) => onChange(e.target.value)} />
  );
}

function LeafWidget({ slotDef, value, onChange }) {
  const kind = value ? value.kind : defaultKindFor(slotDef);
  if (kind === "boolean") {
    return (
      <label className="eb-check">
        <input type="checkbox" checked={!!(value && value.value)}
          onChange={(e) => onChange({ kind: "boolean", value: e.target.checked })} />
        {slotDef.name}
      </label>
    );
  }
  if (kind === "vector") {
    return (
      <TextField placeholder="ex.: 10 20 40 80" value={value ? value.value : ""}
        onChange={(v) => onChange({ kind: "vector", value: v })} />
    );
  }
  if (kind === "unit") {
    const options = slotDef.unitFamilies.flatMap((f) => f.options);
    const cur = value || { kind: "unit", value: 0, unit: options[0] || "" };
    return (
      <span className="eb-row">
        <input className="eb-input eb-num" type="number" value={cur.value}
          onChange={(e) => onChange({ ...cur, value: e.target.value })} />
        <select className="eb-select" value={cur.unit}
          onChange={(e) => onChange({ ...cur, unit: e.target.value })}>
          {slotDef.acceptsNumber && <option value="">(número cru, sem unidade)</option>}
          {options.map((u) => <option key={u} value={u}>{u}</option>)}
        </select>
      </span>
    );
  }
  if (kind === "number") {
    return (
      <input className="eb-input eb-num" type="number" value={value ? value.value : 0}
        onChange={(e) => onChange({ kind: "number", value: e.target.value })} />
    );
  }
  // texto (cobre String/Identifier, e o caso dual texto-ou-numero como
  // Component.select/AirVehicle.initGearPos -- ver serializeLeafValue)
  return (
    <TextField value={value ? value.value : ""} placeholder={slotDef.isReference ? "nome de outro objeto do cenário" : ""}
      onChange={(v) => onChange({ kind: "text", value: v })} />
  );
}

/* -------------------------------- paleta ---------------------------------- */

function Palette({ onDragStartFactory }) {
  const [q, setQ] = useState("");
  const groups = useMemo(() => {
    const t = q.trim().toLowerCase();
    const byOrigin = {};
    EDL_CATALOG.forEach((e) => {
      if (t && !(e.factory.toLowerCase().includes(t) || e.class.toLowerCase().includes(t))) return;
      (byOrigin[e.origin] = byOrigin[e.origin] || []).push(e);
    });
    Object.values(byOrigin).forEach((l) => l.sort((a, b) => a.factory.localeCompare(b.factory)));
    return Object.entries(byOrigin).sort((a, b) => originRank(a[0]) - originRank(b[0]));
  }, [q]);

  return (
    <div className="eb-pane eb-palette">
      <input className="eb-input" type="text" placeholder="Buscar classe…" value={q}
        onChange={(e) => setQ(e.target.value)} aria-label="Buscar classe" />
      <div className="eb-palette-list">
        {groups.map(([origin, list]) => (
          <div key={origin} className="eb-group">
            <div className="eb-group-title">{originLabel(origin)} <span className="eb-muted">({list.length})</span></div>
            {list.map((e) => (
              <div key={e.factory} className="eb-chip" draggable
                title={`${e.factory} (classe C++ ${e.class})`}
                onDragStart={(ev) => { ev.dataTransfer.setData("text/plain", e.factory); onDragStartFactory(e.factory); }}
                onDragEnd={() => onDragStartFactory(null)}>
                {e.factory}
                {e.factory !== e.class && <em className="eb-alias">→{e.class}</em>}
              </div>
            ))}
          </div>
        ))}
        {groups.length === 0 && <p className="eb-muted eb-empty-msg">nenhuma classe bate com “{q}”</p>}
      </div>
    </div>
  );
}

/* ------------------------------ árvore (outline) --------------------------- */

// Fallback sem arrastar -- mesma lista de compatibilidade do drag-and-drop
// (isCompatible), so que via <select>. Existe para acessibilidade e para
// nao deixar a ferramenta inteira refem de um gesto de mouse so.
function AddViaSelect({ slotDef, onPick }) {
  const options = useMemo(() => compatibleFactories(slotDef, EDL_CATALOG, CATALOG_BY_FACTORY), [slotDef]);
  const [v, setV] = useState("");
  if (!options.length) return <span className="eb-muted eb-inline"> (nenhuma classe do catálogo serve aqui)</span>;
  return (
    <span className="eb-inline">
      <select className="eb-select" value={v} onChange={(e) => setV(e.target.value)}>
        <option value="">ou escolha…</option>
        {options.map((f) => <option key={f} value={f}>{f}</option>)}
      </select>
      <button className="eb-btn eb-btn-sm" disabled={!v} onClick={() => { onPick(v); setV(""); }}>Adicionar</button>
    </span>
  );
}

// Vive no painel de propriedades (coluna da direita), pro nó SELECIONADO --
// não recursiona pra dentro dos filhos (isso é o NavTree, na árvore
// central): aqui é só "quais componentes este nó tem, arraste/adicione
// mais, clique num pra ir pra ele" -- ver o "porque" no cabeçalho de
// PropertiesPanel.
function ChildSlotRow({ node, slotDef, dragFactory, selectedId, onSelect, onDrop, onRemoveItem, onRenameKey, onAddText, onChangeText }) {
  const items = node.children[slotDef.name] || [];
  const canAddMore = slotDef.acceptsChildList || items.length === 0;
  const compatibleNow = dragFactory ? isCompatible(dragFactory, slotDef, CATALOG_BY_FACTORY) : null;

  return (
    <div className="eb-slot">
      <div className="eb-slot-name">
        {slotDef.name}
        {slotDef.acceptsChildList ? <span className="eb-muted"> (lista)</span> : null}
      </div>
      {items.map((it) => it.node.isText ? (
        // Item de VALOR ESCALAR (ex.: TacviewOutput.modelMap/typeMap/colorMap
        // -- cada entrada é um texto, não uma classe MIXR; ver o cabeçalho de
        // makeTextLeaf() em edl_builder_core.js).
        <div key={it.node.id} className="eb-node eb-node-text">
          <div className="eb-node-head">
            <input className="eb-key" value={it.key}
              onChange={(e) => onRenameKey(node.id, slotDef.name, it.node.id, e.target.value)} />
            <span className="eb-muted">:</span>
            <input className="eb-input" value={it.node.text}
              onChange={(e) => onChangeText(it.node.id, e.target.value)} />
            <button className="eb-x" title="remover" onClick={() => onRemoveItem(node.id, slotDef.name, it.node.id)}>×</button>
          </div>
        </div>
      ) : (
        <div key={it.node.id} className={"eb-node" + (selectedId === it.node.id ? " eb-node-selected" : "")}>
          <div className="eb-node-head" onClick={() => onSelect(it.node.id)} title="clique para editar este componente">
            {slotDef.acceptsChildList && (
              <input className="eb-key" value={it.key}
                onClick={(e) => e.stopPropagation()}
                onChange={(e) => onRenameKey(node.id, slotDef.name, it.node.id, e.target.value)} />
            )}
            <span className="eb-node-factory">{it.node.factory}</span>
            <span className="eb-muted eb-node-goto">→</span>
            <button className="eb-x" title="remover" onClick={(e) => { e.stopPropagation(); onRemoveItem(node.id, slotDef.name, it.node.id); }}>×</button>
          </div>
        </div>
      ))}
      {canAddMore && (
        <div className={"eb-dropzone" + (compatibleNow === true ? " eb-drop-ok" : compatibleNow === false ? " eb-drop-bad" : "")}
          onDragOver={(e) => { if (dragFactory && isCompatible(dragFactory, slotDef, CATALOG_BY_FACTORY)) e.preventDefault(); }}
          onDrop={(e) => {
            e.preventDefault();
            const factory = e.dataTransfer.getData("text/plain");
            if (factory && isCompatible(factory, slotDef, CATALOG_BY_FACTORY)) onDrop(node.id, slotDef.name, factory);
          }}>
          + arraste uma classe aqui{items.length === 0 ? "" : " (adicionar outra)"}
          <AddViaSelect slotDef={slotDef} onPick={(f) => onDrop(node.id, slotDef.name, f)} />
          {slotDef.acceptsChildList && (
            // Nem todo item de uma lista é uma classe -- ver o comentário de
            // makeTextLeaf() em edl_builder_core.js (TacviewOutput.modelMap e
            // companhia). Sempre oferecido em slot-lista: o tipo de cada item
            // não é decidível pela assinatura do slot sozinha (ver isCompatible()).
            <button className="eb-btn eb-btn-sm" style={{ marginLeft: 6 }}
              onClick={() => onAddText(node.id, slotDef.name)}>+ texto</button>
          )}
        </div>
      )}
    </div>
  );
}

function OutlineNode({ node, dragFactory, selectedId, onSelect, onDrop, onRemoveItem, onRenameKey, onAddText, onChangeText, depth }) {
  const entry = CATALOG_BY_FACTORY[node.factory];
  const childSlots = entry ? entry.slots.filter(isChildSlot) : [];
  if (!childSlots.length) return null;
  return (
    <div className="eb-children" style={{ marginLeft: depth ? 14 : 0 }}>
      {childSlots.map((slotDef) => (
        <ChildSlotRow key={slotDef.name} node={node} slotDef={slotDef} dragFactory={dragFactory}
          selectedId={selectedId} onSelect={onSelect} onDrop={onDrop}
          onRemoveItem={onRemoveItem} onRenameKey={onRenameKey}
          onAddText={onAddText} onChangeText={onChangeText} />
      ))}
    </div>
  );
}

/* ----------------------------- painel de propriedades ---------------------- */

function PropertiesPanel({ node, onChangeSlot }) {
  if (!node) {
    return <div className="eb-pane eb-props"><p className="eb-muted eb-empty-msg">selecione um nó na árvore</p></div>;
  }
  const entry = CATALOG_BY_FACTORY[node.factory];
  if (!entry) {
    return <div className="eb-pane eb-props"><p className="eb-muted eb-empty-msg">classe desconhecida: {node.factory}</p></div>;
  }
  const leafSlots = entry.slots.filter(isLeafSlot);
  return (
    <div className="eb-pane eb-props">
      <div className="eb-props-head">
        <div className="eb-props-title">{node.factory}</div>
        {node.factory !== entry.class && <div className="eb-muted">classe C++: {entry.class}</div>}
        <div className="eb-muted">origem: {originLabel(entry.origin)}</div>
      </div>
      {leafSlots.length === 0 && <p className="eb-muted">esta classe não tem slot de valor direto (só filhos, na árvore).</p>}
      {leafSlots.map((slotDef) => (
        <div key={slotDef.name} className="eb-field">
          <label className="eb-field-label" title={slotDef.comment}>
            {slotDef.name}
            {slotDef.isReference && <span className="eb-ref-badge" title="referência por nome, resolvida em runtime">#</span>}
          </label>
          <LeafWidget slotDef={slotDef} value={node.slotValues[slotDef.name]}
            onChange={(v) => onChangeSlot(node.id, slotDef.name, v)} />
        </div>
      ))}
    </div>
  );
}

/* --------------------------------- exportar -------------------------------- */

function ExportPanel({ root }) {
  const [warn, setWarn] = useState(null);
  const text = useMemo(() => projectToEdl(root, CATALOG_BY_FACTORY), [root]);

  const download = () => {
    if (!isAscii(text)) {
      setWarn("Exportação BLOQUEADA: o cenário tem caractere fora de ASCII (acento, etc.). " +
        "Um único caractere assim, em qualquer lugar do arquivo, faz o parser real do MIXR " +
        "rejeitar o arquivo inteiro. Troque o texto pelo equivalente sem acento e exporte de novo.");
      return;
    }
    setWarn(null);
    const blob = new Blob([text], { type: "text/plain" });
    const a = document.createElement("a");
    a.href = URL.createObjectURL(blob);
    a.download = "scenario.edl";
    a.click();
    URL.revokeObjectURL(a.href);
  };

  return (
    <div className="eb-pane eb-export">
      <div className="eb-export-head">
        <span className="eb-props-title">Pré-visualização .edl</span>
        <button className="eb-btn" disabled={!root} onClick={download}>Exportar .edl</button>
      </div>
      {warn && <p className="eb-warn">{warn}</p>}
      <pre className="eb-edl-preview">{text || "(cenário vazio -- arraste uma classe para começar)"}</pre>
    </div>
  );
}

/* ----------------------------------- App ----------------------------------- */

const CSS = `
.eb { --paper:#E6E9E3; --panel:#DCE0D9; --ink:#16232E; --muted:#6E7A76;
  --rule:#C6CDC3; --hot:#B4661E; --ok:#4A6B4F; --bad:#9C3B3B;
  --mono: ui-monospace,'JetBrains Mono','SF Mono',Menlo,monospace;
  --sans: 'Inter',system-ui,-apple-system,sans-serif;
  background:var(--paper); color:var(--ink); font-family:var(--sans);
  font-size:13.5px; line-height:1.5; min-height:100vh; color-scheme: light; }
.eb[data-theme="dark"] { --paper:#181C19; --panel:#232722; --ink:#E7EAE4; --muted:#8B968E;
  --rule:#3A413B; --hot:#D98A4A; --ok:#7FBE8B; --bad:#E08A8A; color-scheme: dark; }
.eb-bar { position:sticky; top:0; z-index:5; background:var(--paper); border-bottom:1px solid var(--rule);
  padding:10px 18px 8px; display:flex; justify-content:space-between; align-items:center; gap:14px; flex-wrap:wrap; }
.eb-h1 { font-size:16px; font-weight:600; margin:0; letter-spacing:-0.01em; }
.eb-sub { font-size:12px; color:var(--muted); margin:2px 0 0; }
.eb-btn { font:inherit; font-size:12.5px; padding:5px 12px; cursor:pointer;
  border:1px solid var(--rule); background:transparent; color:var(--ink); border-radius:2px; }
.eb-btn:hover:not(:disabled) { border-color:var(--ink); }
.eb-btn:disabled { opacity:0.5; cursor:default; }
.eb-btn-sm { font-size:11px; padding:2px 8px; }
.eb-body { padding:12px 18px 24px; display:grid; grid-template-columns: 260px 1fr 320px; gap:12px; align-items:start; }
.eb-pane { border:1px solid var(--rule); border-radius:2px; background:var(--panel); padding:10px; }
.eb-palette { max-height: 78vh; overflow:auto; position:sticky; top:56px; }
.eb-palette-list { margin-top:8px; }
.eb-group-title { font-weight:600; font-size:11.5px; text-transform:uppercase; letter-spacing:.03em;
  color:var(--muted); margin:10px 0 4px; }
.eb-chip { font-family:var(--mono); font-size:12px; padding:3px 6px; margin:2px 0; border:1px solid var(--rule);
  border-radius:2px; background:var(--paper); cursor:grab; display:flex; justify-content:space-between; gap:6px; }
.eb-chip:hover { border-color:var(--hot); }
.eb-alias { color:var(--muted); font-style:normal; font-size:10.5px; }
.eb-tree { min-height: 200px; }
.eb-props { position:sticky; top:56px; max-height:78vh; overflow:auto; }
.eb-props-title { font-weight:600; font-family:var(--mono); }
.eb-field { margin:9px 0; }
.eb-field-label { display:block; font-size:12px; color:var(--muted); margin-bottom:2px; }
.eb-ref-badge { color:var(--hot); margin-left:4px; }
.eb-input { font:inherit; font-size:12.5px; padding:4px 6px; border:1px solid var(--rule); border-radius:2px;
  background:var(--paper); color:var(--ink); width: 100%; box-sizing:border-box; }
.eb-num { width: 110px; }
.eb-select { font:inherit; font-size:12.5px; padding:3px 4px; border:1px solid var(--rule); border-radius:2px;
  background:var(--paper); color:var(--ink); }
.eb-row { display:flex; gap:6px; align-items:center; }
.eb-check { display:flex; gap:6px; align-items:center; font-size:12.5px; }
.eb-muted { color:var(--muted); }
.eb-inline { display:inline-flex; gap:6px; align-items:center; margin-left:6px; }
.eb-empty-msg { font-size:12.5px; }
.eb-root-drop { border:2px dashed var(--rule); border-radius:4px; padding:40px 20px; text-align:center; color:var(--muted); }
.eb-root-drop[data-active="1"] { border-color:var(--hot); color:var(--ink); }
.eb-slot { margin: 6px 0 6px 10px; border-left:2px solid var(--rule); padding-left:8px; }
.eb-slot-name { font-family:var(--mono); font-size:11.5px; color:var(--muted); }
.eb-node { margin:4px 0; border:1px solid var(--rule); border-radius:2px; background:var(--paper); }
.eb-node-selected { border-color:var(--hot); }
.eb-node-head { display:flex; align-items:center; gap:6px; padding:4px 6px; cursor:pointer; font-family:var(--mono); font-size:12px; }
.eb-node-text .eb-node-head { cursor:default; }
.eb-node-text .eb-node-head .eb-input { flex:1; width:auto; }
.eb-node-factory { font-weight:600; }
.eb-key { font-family:var(--mono); font-size:11.5px; width:70px; padding:2px 4px; border:1px solid var(--rule); border-radius:2px; }
.eb-x { margin-left:auto; border:none; background:transparent; color:var(--bad); cursor:pointer; font-size:14px; line-height:1; }
.eb-dropzone { font-size:11.5px; color:var(--muted); border:1px dashed var(--rule); border-radius:2px; padding:5px 8px; margin:4px 0; }
.eb-dropzone.eb-drop-ok { border-color:var(--ok); color:var(--ok); }
.eb-dropzone.eb-drop-bad { border-color:var(--bad); color:var(--bad); opacity:0.6; }
.eb-export { grid-column: 1 / -1; margin-top: 4px; }
.eb-export-head { display:flex; justify-content:space-between; align-items:center; }
.eb-edl-preview { font-family:var(--mono); font-size:11.5px; background:var(--paper); border:1px solid var(--rule);
  border-radius:2px; padding:10px; max-height:280px; overflow:auto; white-space:pre; margin-top:8px; }
.eb-warn { color:var(--bad); font-size:12.5px; }
`;

export default function App() {
  const [root, setRoot] = useState(null);
  const [selectedId, setSelectedId] = useState(null);
  const [dragFactory, setDragFactory] = useState(null);
  const [theme, setTheme] = useState(() => {
    try { return localStorage.getItem("mx-edl-builder-theme") === "dark" ? "dark" : "light"; } catch { return "light"; }
  });
  useEffect(() => { try { localStorage.setItem("mx-edl-builder-theme", theme); } catch { /* sem storage -- ok */ } }, [theme]);

  const selectedNode = useMemo(() => findNode(root, selectedId), [root, selectedId]);

  const handleDrop = useCallback((nodeId, slotName, factory) => {
    setRoot((r) => updateNode(r, nodeId, (n) => {
      const slotDef = CATALOG_BY_FACTORY[n.factory].slots.find((s) => s.name === slotName);
      const child = makeNode(factory);
      const existing = n.children[slotName] || [];
      const key = slotDef.acceptsChildList ? String(existing.length + 1) : "1";
      const items = slotDef.acceptsChildList ? [...existing, { key, node: child }] : [{ key, node: child }];
      return { ...n, children: { ...n.children, [slotName]: items } };
    }));
  }, []);

  const handleRemoveItem = useCallback((nodeId, slotName, childId) => {
    setRoot((r) => {
      if (r && r.id === childId) return null;
      return updateNode(r, nodeId, (n) => ({
        ...n,
        children: { ...n.children, [slotName]: (n.children[slotName] || []).filter((it) => it.node.id !== childId) },
      }));
    });
    setSelectedId((s) => (s === childId ? null : s));
  }, []);

  const handleRenameKey = useCallback((nodeId, slotName, childId, newKey) => {
    setRoot((r) => updateNode(r, nodeId, (n) => ({
      ...n,
      children: {
        ...n.children,
        [slotName]: (n.children[slotName] || []).map((it) => (it.node.id === childId ? { ...it, key: newKey } : it)),
      },
    })));
  }, []);

  const handleChangeSlot = useCallback((nodeId, slotName, value) => {
    setRoot((r) => updateNode(r, nodeId, (n) => ({ ...n, slotValues: { ...n.slotValues, [slotName]: value } })));
  }, []);

  // Nem todo item de uma lista é uma classe -- ver makeTextLeaf() em
  // edl_builder_core.js. onAddText usa a MESMA regra de chave que
  // handleDrop (auto-numerada, 1-based) pra continuar batendo com o que a
  // gramática produziria pra um item anônimo.
  const handleAddText = useCallback((nodeId, slotName) => {
    setRoot((r) => updateNode(r, nodeId, (n) => {
      const existing = n.children[slotName] || [];
      const items = [...existing, { key: String(existing.length + 1), node: makeTextLeaf("") }];
      return { ...n, children: { ...n.children, [slotName]: items } };
    }));
  }, []);

  const handleChangeText = useCallback((leafId, text) => {
    setRoot((r) => updateNode(r, leafId, (n) => ({ ...n, text })));
  }, []);

  const handleNew = () => {
    if (root && !window.confirm("Descartar o cenário atual?")) return;
    setRoot(null); setSelectedId(null);
  };

  // "Excluir a classe raiz" -- descarta a árvore inteira (não há Station
  // "vazia": a raiz É o cenário). Mesma confirmação de "Novo", só que
  // disparada a partir do próprio nó raiz, não da barra de ferramentas --
  // mais descobrível do que só o botão "Novo" genérico.
  const handleRemoveRoot = handleNew;

  const handleLoadPreset = (build, label) => {
    if (root && !window.confirm(`Descartar o cenário atual e carregar o preset "${label}"?`)) return;
    setRoot(build());
    setSelectedId(null);
  };

  const handleSaveProject = () => {
    const blob = new Blob([JSON.stringify(root, null, 2)], { type: "application/json" });
    const a = document.createElement("a");
    a.href = URL.createObjectURL(blob);
    a.download = "cenario.mixr-edl-builder.json";
    a.click();
    URL.revokeObjectURL(a.href);
  };

  const fileInputRef = useRef(null);
  const handleOpenProject = (file) => {
    const reader = new FileReader();
    reader.onload = () => {
      try {
        const parsed = JSON.parse(reader.result);
        resetIdCounter(1 + maxId(parsed, 0));
        setRoot(parsed);
        setSelectedId(null);
      } catch (err) {
        window.alert("Não consegui abrir este projeto: " + err.message);
      }
    };
    reader.readAsText(file);
  };

  return (
    <div className="eb" data-theme={theme}>
      <style>{CSS}</style>
      <div className="eb-bar">
        <div>
          <h1 className="eb-h1">MIXR — Editor gráfico de cenário .edl</h1>
          <p className="eb-sub">{EDL_CATALOG.length} classes catalogadas, de todas as factories que este repositório de fato carrega</p>
        </div>
        <div className="eb-row">
          <button className="eb-btn" onClick={handleNew}>Novo</button>
          <button className="eb-btn" onClick={handleSaveProject} disabled={!root}>Salvar projeto</button>
          <button className="eb-btn" onClick={() => fileInputRef.current && fileInputRef.current.click()}>Abrir projeto</button>
          <input ref={fileInputRef} type="file" accept="application/json" style={{ display: "none" }}
            onChange={(e) => { if (e.target.files[0]) handleOpenProject(e.target.files[0]); e.target.value = ""; }} />
          <button className="eb-btn"
            title="Tudo de src/poc/dis/bandit/configs/scenario.edl, exceto os players -- pronto pra arrastar a própria aeronave em cima"
            onClick={() => handleLoadPreset(buildBanditPreset, "bandit (sem players)")}>
            Preset: bandit
          </button>
          <button className="eb-btn" onClick={() => setTheme((t) => (t === "light" ? "dark" : "light"))}>
            {theme === "light" ? "☾ escuro" : "☀ claro"}
          </button>
        </div>
      </div>
      <div className="eb-body">
        <Palette onDragStartFactory={setDragFactory} />
        <div className="eb-pane eb-tree">
          {!root ? (
            <div className="eb-root-drop" data-active={dragFactory ? "1" : "0"}
              onDragOver={(e) => { if (dragFactory && isCompatible(dragFactory, ROOT_SLOT_DEF, CATALOG_BY_FACTORY)) e.preventDefault(); }}
              onDrop={(e) => {
                e.preventDefault();
                const factory = e.dataTransfer.getData("text/plain");
                if (factory && isCompatible(factory, ROOT_SLOT_DEF, CATALOG_BY_FACTORY)) { setRoot(makeNode(factory)); }
              }}>
              arraste uma classe que implemente Station aqui para começar (ex.: Station ou ClockStation)
              <div style={{ marginTop: 10 }}>
                <AddViaSelect slotDef={ROOT_SLOT_DEF} onPick={(f) => setRoot(makeNode(f))} />
              </div>
            </div>
          ) : (
            <>
              <div className={"eb-node" + (selectedId === root.id ? " eb-node-selected" : "")}>
                <div className="eb-node-head" onClick={() => setSelectedId(root.id)}>
                  <span className="eb-node-factory">{root.factory}</span>
                  <span className="eb-muted">(raiz)</span>
                  <button className="eb-x" title="excluir a classe raiz (descarta o cenário inteiro)"
                    onClick={(e) => { e.stopPropagation(); handleRemoveRoot(); }}>×</button>
                </div>
              </div>
              <OutlineNode node={root} dragFactory={dragFactory} selectedId={selectedId}
                onSelect={setSelectedId} onDrop={handleDrop}
                onRemoveItem={handleRemoveItem} onRenameKey={handleRenameKey}
                onAddText={handleAddText} onChangeText={handleChangeText} depth={0} />
            </>
          )}
        </div>
        <PropertiesPanel node={selectedNode} onChangeSlot={handleChangeSlot} />
        <ExportPanel root={root} />
      </div>
    </div>
  );
}
