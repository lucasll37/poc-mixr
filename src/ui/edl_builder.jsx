import React, { useState, useMemo, useRef, useEffect, useCallback } from "react";

/* ==================================================================== *
 * MIXR — Editor gráfico de cenário .edl
 *
 * EDL_CATALOG é GERADO por src/ui/scripts/generate_edl_catalog.py e
 * injetado num <script> próprio por src/ui/scripts/compile.js, ANTES deste
 * arquivo -- ver o comentário de `dataScript` em compile.js. Cobre TODAS as
 * factories que app/src/mixr_factory.cpp de fato encadeia (base/models/
 * terrain/interop-dis/linkage/recorder/simulation), mais libs/x* e os
 * plugins deste repositório -- não é um recorte curado.
 *
 * A lógica PURA (índice do catálogo, compatibilidade de slot, o modelo de
 * dados do projeto e o serializador para texto .edl) mora em
 * src/ui/edl_builder_core.js, não aqui -- compile.js concatena os dois no
 * MESMO <script>, então as funções de lá (buildCatalogIndex, isCompatible,
 * makeNode, projectToEdl, primaryRolesFor, roleFillStatus,
 * emptyChildSlots, ...) já chegam em escopo, sem `import`. Ver o
 * cabeçalho daquele arquivo para o porquê (é o que permite testá-la em
 * Node puro, sem Babel/React/DOM, em src/ui/edl_builder.test.js).
 *
 * Escopo original da v1 (CRIAR um cenário do zero -- arrastar classes da
 * paleta, preencher campos, exportar .edl) foi ampliado: o botão
 * "Carregar .edl" abre um .edl/.edl.in REAL qualquer -- não só o preset
 * embutido -- e mapeia TUDO para esta mesma árvore, pronto pra editar e
 * reexportar. O parser mora em src/ui/edl_parser_core.js (concatenado por
 * compile.js igual a este arquivo), a MESMA lógica que já gerava o preset
 * em build-time -- ver `handleOpenEdl()`/`parseEdlDocument()` mais abaixo.
 * Fábrica ou slot que o catálogo ainda não conhece (classe em
 * desenvolvimento, plugin de terceiro nunca introspectado, erro de
 * digitação) NUNCA é descartado -- fica preservado como valor bruto
 * (kind "raw", ver PropertiesPanel/UncatalogedSlotsSection), editável e
 * reexportado byte-fiel, com um aviso explícito em vez de sumir em
 * silêncio. `@include:frag@` não é suportado por este caminho (ver o
 * comentário de `handleOpenEdl()`); a ferramenta continua abrindo com a
 * árvore VAZIA por padrão (decisão explícita: carregar um cenário grande
 * sozinho escondia com que se estava mexendo de fato) -- o botão "Carregar
 * preset" continua oferecendo, sob demanda, o cenário de MAIS componentes
 * do repositório (ver o comentário de `loadPresetTree()`/EDL_PRESET, mais
 * abaixo).
 *
 * A árvore central é uma árvore DE VERDADE (TreeNode), não mais um outline
 * de navegação separado de um painel de edição: cada nó é um cartão
 * retrátil, com fundo em cor suave por ORIGEM (models/base/terrain/.../
 * plugin:*) e, para qualquer classe derivada de Player (Aircraft,
 * GroundVehicle, ...), uma seção "Sistemas principais" com um cartão-
 * placeholder tracejado para cada um dos ~10 papéis (dynamicsModel/pilot/
 * navigation/datalink/radio/gimbal/rfSensor/irSystem/onboardComputer/
 * storesMgr) que Player::updateSystemPointers() resolve por TIPO -- não
 * por slot, ver o comentário de primaryRolesFor()/roleFillStatus() em
 * edl_builder_core.js -- e que por isso nunca apareciam na árvore antiga.
 * Qualquer outro slot-filho vazio (ex.: Station.dataRecorder) também ganha
 * um placeholder genérico, arrastável, com a mesma aparência. O painel de
 * propriedades (coluna da direita) encolheu para só os campos de valor
 * direto do nó selecionado -- editar estrutura (arrastar/remover/
 * renomear) agora acontece direto na árvore.
 * ==================================================================== */

const CATALOG_BY_FACTORY = buildCatalogIndex(EDL_CATALOG);

// A raiz do cenário TEM que ser algo que implementa Station -- nunca
// "qualquer classe" (era assim antes: objectTypes:["Object"], que qualquer
// coisa satisfaz). isCompatible() já sabe aceitar subclasse (ClockStation
// deriva de Station), então isto sozinho já cobre as duas raízes reais
// deste repositório sem precisar listar as duas à mão.
const ROOT_SLOT_DEF = { objectTypes: ["Station"], acceptsChildList: false };

// Grupos de cor suave por origem -- só o rótulo curto que vira classe CSS
// (eb-origin-<classe>); os tons de verdade (claro/escuro) estão no bloco
// CSS mais abaixo. 'interop/dis' tem barra (não pode virar classe CSS
// direto) e cada 'plugin:<nome>' é um valor DIFERENTE por plugin -- os
// dois caem no mesmo balde visual ("dis"/"plugin"), já que a distinção
// fina não ajuda a leitura rápida da árvore.
const ORIGIN_CSS_CLASS = {
  models: "models", base: "base", terrain: "terrain", "interop/dis": "dis",
  linkage: "linkage", recorder: "recorder", simulation: "simulation", libs: "libs",
};
function originCssClass(origin) {
  if (!origin) return "other";
  if (ORIGIN_CSS_CLASS[origin]) return ORIGIN_CSS_CLASS[origin];
  if (origin.startsWith("plugin:")) return "plugin";
  return "other";
}

// {filled, total} dos slots de FOLHA do nó -- o "N/M" no cabeçalho do
// cartão, pra dar uma noção de completude sem precisar abrir o painel de
// propriedades.
function nodeSummary(node, byFactory) {
  const entry = byFactory[node.factory];
  if (!entry) return { filled: 0, total: 0 };
  const leaves = entry.slots.filter(isLeafSlot);
  const filled = leaves.filter((s) => !isEmptyLeafValue(node.slotValues[s.name])).length;
  return { filled, total: leaves.length };
}

// Quantos "esperados, mas ausentes" este nó tem AGORA -- vira o badge "N
// pendentes" no cabeçalho, visível mesmo com o cartão COLAPSADO, pra
// convidar a abrir sem precisar adivinhar. A REGRA em si (papéis vazios +
// slots-filho vazios, sem contar 'components' duas vezes) mora em
// nodePendencies() (edl_builder_core.js) -- a MESMA usada pelo resumo
// tree-wide da aba "Pendências" (collectPendencies), pra badge por cartão e
// resumo nunca divergirem sobre o que conta como pendência.
function missingCount(node, byFactory) {
  return nodePendencies(node, byFactory).length;
}

// Profundidade default de expansão ao carregar um cenário -- raiz e seus
// filhos diretos abertos, o resto colapsado (evita que os 53 componentes
// do cenário built-in_mixr_1 virem uma parede de cartões na primeira
// olhada). maxDepth conta em NÓS reais (não em nível de slot).
function defaultExpandedIds(root, maxDepth) {
  const ids = new Set();
  function walk(node, depth) {
    if (!node || node.isText) return;
    if (depth <= maxDepth) ids.add(node.id);
    for (const items of Object.values(node.children || {})) {
      for (const it of items) walk(it.node, depth + 1);
    }
  }
  walk(root, 0);
  return ids;
}

/* --------------------------------- widgets -------------------------------- */

function TextField({ value, onChange, placeholder }) {
  return (
    <input className="eb-input" type="text" value={value} placeholder={placeholder}
      onChange={(e) => onChange(e.target.value)} />
  );
}

function LeafWidget({ slotDef, value, onChange }) {
  const kind = value ? value.kind : defaultKindFor(slotDef);
  // Valor de slot NAO CATALOGADO (kind "raw", ver edl_parser_core.js) --
  // texto EXATO que vai pro .edl, sem adivinhar aspas/tipo. So aparece
  // quando o proprio 'value' ja tem kind "raw" (carregado de um .edl real,
  // ou criado a mao via "+ slot" no painel) -- nunca e' o default de um
  // slot novo arrastado da paleta (defaultKindFor nunca devolve "raw").
  if (kind === "raw") {
    return (
      <TextField value={value ? value.raw : ""}
        placeholder="valor bruto -- texto exato que deve sair no .edl"
        onChange={(v) => onChange({ kind: "raw", raw: v })} />
    );
  }
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
              <div key={e.factory}
                className={`eb-chip eb-origin-${originCssClass(e.origin)}` + (e.runtimeOnly ? " eb-chip-runtime" : "")}
                draggable
                title={`${e.factory} (classe C++ ${e.class})` + (e.runtimeOnly ? " -- descoberta em runtime, plugin de terceiros sem fonte" : "")}
                onDragStart={(ev) => { ev.dataTransfer.setData("text/plain", e.factory); onDragStartFactory(e.factory); }}
                onDragEnd={() => onDragStartFactory(null)}>
                {e.factory}
                {e.factory !== e.class && <em className="eb-alias">→{e.class}</em>}
                {e.runtimeOnly && <em className="eb-alias" title="plugin de terceiros, descoberto em runtime"> ⚙</em>}
              </div>
            ))}
          </div>
        ))}
        {groups.length === 0 && <p className="eb-muted eb-empty-msg">nenhuma classe bate com “{q}”</p>}
      </div>
    </div>
  );
}

/* ---------------------------- placeholder / drop --------------------------- */

// O "quadro" que representa um lugar onde o cenário ESPERA algo mas ainda
// não tem -- borda tracejada, fundo desaturado (nunca a mesma cor sólida
// de um cartão preenchido), continua sendo alvo de arrastar-e-soltar
// (reaproveita isCompatible/AddViaSelect sem mudança nenhuma) mais o
// fallback de <select> pra quem não usa mouse.
// `onAddText`, quando dado (só faz sentido em slot-LISTA -- um papel
// primário/slot de objeto único, como `dynamicsModel:`, nunca aceita um
// valor de texto puro no lugar de um componente), acrescenta o botão
// "+ texto" ao lado do <select> -- o mecanismo que TacviewOutput.typeMap/
// colorMap/modelMap, PluginModule.provides e companhia exigem (ver
// makeTextLeaf() em edl_builder_core.js): nenhuma classe do catálogo serve
// nesses slots (são tabelas nome→string, não listas de componente), então
// texto é o ÚNICO jeito de preenchê-los.
function PlaceholderCard({ label, slotDef, dragFactory, onDrop, onAddText, compact }) {
  const compatibleNow = dragFactory != null ? isCompatible(dragFactory, slotDef, CATALOG_BY_FACTORY) : null;
  return (
    <div
      className={"eb-placeholder" + (compact ? " eb-placeholder-compact" : "") +
        (compatibleNow === true ? " eb-drop-ok" : compatibleNow === false ? " eb-drop-bad" : "")}
      onDragOver={(e) => { if (dragFactory && isCompatible(dragFactory, slotDef, CATALOG_BY_FACTORY)) e.preventDefault(); }}
      onDrop={(e) => {
        e.preventDefault();
        const factory = e.dataTransfer.getData("text/plain");
        if (factory && isCompatible(factory, slotDef, CATALOG_BY_FACTORY)) onDrop(factory);
      }}>
      <span className="eb-placeholder-label">{label}</span>
      <AddViaSelect slotDef={slotDef} onPick={onDrop} />
      {onAddText && slotDef.textOnly && (
        <button type="button" className="eb-btn eb-btn-sm eb-btn-addtext" onClick={onAddText}>+ texto</button>
      )}
    </div>
  );
}

// Fallback sem arrastar -- mesma lista de compatibilidade do drag-and-drop
// (isCompatible), so que via <select>. Existe para acessibilidade e para
// nao deixar a ferramenta inteira refem de um gesto de mouse so.
function AddViaSelect({ slotDef, onPick }) {
  const options = useMemo(() => compatibleFactories(slotDef, EDL_CATALOG, CATALOG_BY_FACTORY), [slotDef]);
  const [v, setV] = useState("");
  if (!options.length) {
    // slotDef.uncataloged (slot que o CATALOGO NAO CONHECE -- fabrica
    // inteiramente desconhecida, ou classe conhecida com um slot a mais,
    // ver TreeNode) e slotDef.textOnly (TacviewOutput.typeMap,
    // PluginModule.provides, ...) chegam aqui pelo MESMO motivo mecânico
    // (objectTypes vazio -- isCompatible já recusa toda candidata), mas por
    // razões DIFERENTES: um nunca teve classe nenhuma por design, o outro
    // é so' porque a ferramenta não sabe o tipo de verdade -- mensagens
    // diferentes evitam confundir as duas.
    return (
      <span className="eb-muted eb-inline">
        {slotDef.uncataloged
          ? " (slot não catalogado -- use “+ texto”)"
          : slotDef.textOnly
            ? " (só aceita texto -- use “+ texto”)"
            : " (nenhuma classe do catálogo é compatível aqui)"}
      </span>
    );
  }
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

/* ---------------------------------- árvore --------------------------------- */

// Um item de uma lista/slot: OU um valor de texto puro (TacviewOutput.
// modelMap/typeMap/colorMap e companhia -- ver makeTextLeaf() em
// edl_builder_core.js), OU um nó de verdade, que recursiona em <TreeNode>.
// 'isList' decide se a CHAVE do item é editável (só faz sentido numa
// PairStream de verdade -- um slot de objeto único tem uma chave fixa
// "1", sem significado nenhum pro .edl).
function TreeItem({ parentId, slotName, isList, item, ui, actions }) {
  const { key, node } = item;
  if (node.isText) {
    return (
      <div className="eb-tree-textitem">
        {isList && (
          <input className="eb-key" value={key} onClick={(e) => e.stopPropagation()}
            onChange={(e) => actions.onRenameKey(parentId, slotName, node.id, e.target.value)} />
        )}
        <span className="eb-muted">:</span>
        <input className="eb-input" value={node.text}
          onChange={(e) => actions.onChangeText(node.id, e.target.value)} />
        <button className="eb-x" title="remover" onClick={() => actions.onRemoveItem(parentId, slotName, node.id)}>×</button>
      </div>
    );
  }
  return (
    <TreeNode node={node} ui={ui} actions={actions}
      keyBadge={isList ? key : null}
      onRenameKeyLocal={isList ? (v) => actions.onRenameKey(parentId, slotName, node.id, v) : null}
      onRemoveLocal={() => actions.onRemoveItem(parentId, slotName, node.id)} />
  );
}

// A seção "Sistemas principais" -- só aparece em nós cuja classe deriva de
// Player (via primaryRolesFor, que já vem achatado por herança do
// catálogo). Cada papel é OU o componente real que o preenche (achado por
// TIPO, não pela chave -- ver roleFillStatus()) OU um PlaceholderCard
// convidando a arrastar a classe-base esperada. Os itens de 'components'
// que NÃO satisfazem nenhum papel (sensores extras, etc.) entram à parte,
// em "outros componentes".
function RoleSection({ node, roles, ui, actions }) {
  const items = node.children.components || [];
  const claimedIds = new Set(
    roles.map((r) => roleFillStatus(node, r, ui.byFactory)).filter(Boolean).map((it) => it.node.id)
  );
  const others = items.filter((it) => !claimedIds.has(it.node.id));
  const componentsSlotDef = ui.byFactory[node.factory].slots.find((s) => s.name === "components");

  return (
    <div className="eb-role-frame">
      <div className="eb-role-frame-title">Sistemas principais</div>
      <div className="eb-branch-list">
        {roles.map((role) => {
          const found = roleFillStatus(node, role, ui.byFactory);
          return (
            <div key={role.role} className="eb-role-row eb-branch-item">
              <div className="eb-role-label">{role.role}</div>
              {found ? (
                <TreeItem parentId={node.id} slotName="components" isList item={found} ui={ui} actions={actions} />
              ) : (
                <PlaceholderCard
                  label={`nenhum ${role.baseClass} encontrado`}
                  slotDef={{ objectTypes: [role.baseClass], acceptsChildList: false }}
                  dragFactory={ui.dragFactory}
                  onDrop={(factory) => actions.onDrop(node.id, "components", factory, role.role)} />
              )}
            </div>
          );
        })}
      </div>
      <div className="eb-role-others">
        <div className="eb-role-label eb-muted">outros componentes</div>
        <div className="eb-branch-list">
          {others.map((it) => (
            <div key={it.node.id} className="eb-branch-item">
              <TreeItem parentId={node.id} slotName="components" isList item={it} ui={ui} actions={actions} />
            </div>
          ))}
          {componentsSlotDef && (
            <div className="eb-branch-item">
              <PlaceholderCard compact
                label={others.length === 0 && items.length === 0 ? "+ arraste um componente aqui" : "+ adicionar outro"}
                slotDef={componentsSlotDef} dragFactory={ui.dragFactory}
                onDrop={(factory) => actions.onDrop(node.id, "components", factory)} />
            </div>
          )}
        </div>
      </div>
    </div>
  );
}

// Um slot-filho QUALQUER, fora do mecanismo especial de papéis acima (ex.:
// Station.dataRecorder, Station.networks) -- sempre mostrado, mesmo vazio:
// é isso que torna "o que este cenário aceita aqui" visível ANTES de
// alguém arrastar algo, não só depois.
function ChildSlotSection({ node, slotDef, ui, actions }) {
  const items = node.children[slotDef.name] || [];
  const isList = slotDef.acceptsChildList;
  return (
    <div className={"eb-slot-frame" + (slotDef.uncataloged ? " eb-slot-frame-uncataloged" : "")}>
      <div className="eb-slot-frame-title">
        {slotDef.name}{isList ? " (lista)" : ""}
        {slotDef.uncataloged && <span className="eb-unknown-badge" title="slot que o catalogo nao conhece -- preservado como esta, editavel so como texto"> não catalogado</span>}
      </div>
      <div className="eb-branch-list">
        {items.map((it) => (
          <div key={it.node.id} className="eb-branch-item">
            <TreeItem parentId={node.id} slotName={slotDef.name} isList={isList} item={it} ui={ui} actions={actions} />
          </div>
        ))}
        {(isList || items.length === 0) && (
          <div className="eb-branch-item">
            <PlaceholderCard
              compact={items.length > 0}
              label={items.length > 0
                ? "+ adicionar outro"
                : slotDef.uncataloged
                  ? "slot não catalogado (nome: valor)"
                  : slotDef.textOnly
                    ? "só aceita texto (nome: valor)"
                    : `nenhum ${slotDef.objectTypes.join("/") || "componente"} encontrado`}
              slotDef={slotDef} dragFactory={ui.dragFactory}
              onDrop={(factory) => actions.onDrop(node.id, slotDef.name, factory)}
              onAddText={() => actions.onAddText(node.id, slotDef.name)} />
          </div>
        )}
      </div>
    </div>
  );
}

// O CARTÃO -- unidade visual da árvore nova. Cabeçalho: chevron (se tem
// filho), nome da fábrica, chave (se dentro de uma lista), badge de
// origem, "N/M" de campos preenchidos e "N pendentes" quando há papel/slot
// vazio. Corpo (só quando expandido): a seção de papéis (se a classe for
// um Player) e depois cada slot-filho restante, cada um seu próprio
// quadro com fundo suave -- a "moldura" que agrupa visualmente o que
// pertence a cada slot.
function TreeNode({ node, ui, actions, keyBadge, onRenameKeyLocal, onRemoveLocal, isRoot }) {
  const entry = ui.byFactory[node.factory];
  const originClass = originCssClass(entry ? entry.origin : "");
  const selected = ui.selectedId === node.id;
  const expanded = isRoot || ui.expandedIds.has(node.id);
  const childSlots = entry ? entry.slots.filter(isChildSlot) : [];
  // Slots-FILHO que o catalogo NAO CONHECE -- fabrica inteiramente
  // desconhecida (entry undefined, childSlots ja vazio) ou uma classe
  // conhecida com um slot a mais (typo, classe em desenvolvimento, plugin
  // de terceiro nunca introspectado). Sem isto, um .edl real carregado com
  // esse conteudo teria os DADOS preservados (ver edl_parser_core.js) mas
  // INVISIVEIS na arvore -- exatamente o oposto do que "mapear tudo pra
  // edicao" promete. Reaproveita ChildSlotSection/PlaceholderCard/
  // AddViaSelect por inteiro, so com um slotDef SINTETICO
  // (objectTypes:[] -- nenhuma classe do catalogo "serve" aqui, por nao
  // saber o tipo de verdade -- que ja desliga o drag-drop tipado com
  // honestidade, o mesmo tratamento que um slot textOnly real ja recebe).
  const knownChildNames = new Set(childSlots.map((s) => s.name));
  const extraChildNames = Object.keys(node.children || {}).filter((n) => !knownChildNames.has(n));
  const hasChildren = childSlots.length > 0 || extraChildNames.length > 0;
  const { filled, total } = nodeSummary(node, ui.byFactory);
  const roles = entry ? primaryRolesFor(node.factory, ui.byFactory) : [];
  const pending = entry ? missingCount(node, ui.byFactory) : 0;

  return (
    <div className={`eb-card eb-origin-${originClass}` + (selected ? " eb-card-selected" : "") + (!entry ? " eb-card-uncataloged" : "")}>
      <div className="eb-card-head" onClick={() => actions.onSelect(node.id)} title="clique para editar este componente">
        {hasChildren ? (
          <button className="eb-chevron" onClick={(e) => { e.stopPropagation(); actions.onToggleExpand(node.id); }}
            title={expanded ? "recolher" : "expandir"}>
            {expanded ? "▾" : "▸"}
          </button>
        ) : <span className="eb-chevron-spacer" />}
        {keyBadge != null && (
          <input className="eb-key" value={keyBadge} onClick={(e) => e.stopPropagation()}
            onChange={(e) => onRenameKeyLocal(e.target.value)} />
        )}
        <span className={`eb-origin-dot eb-origin-${originClass}`} title={originLabel(entry ? entry.origin : "")} />
        <span className="eb-node-factory">{node.factory}</span>
        {!entry && <span className="eb-unknown-badge" title="fabrica nao catalogada -- slots preservados como valor bruto, editaveis no painel de propriedades">?</span>}
        {isRoot && <span className="eb-muted">(raiz)</span>}
        {total > 0 && <span className="eb-fill-count" title="campos de valor preenchidos / total">{filled}/{total}</span>}
        {pending > 0 && (
          <span className="eb-missing-badge" title="componentes esperados que ainda faltam">{pending} pendente{pending > 1 ? "s" : ""}</span>
        )}
        {onRemoveLocal && (
          <button className="eb-x" title="remover" onClick={(e) => { e.stopPropagation(); onRemoveLocal(); }}>×</button>
        )}
      </div>
      {expanded && (entry || extraChildNames.length > 0) && (
        <div className="eb-card-body">
          {roles.length > 0 && <RoleSection node={node} roles={roles} ui={ui} actions={actions} />}
          {childSlots
            .filter((s) => !(roles.length > 0 && s.name === "components"))
            .map((slotDef) => (
              <ChildSlotSection key={slotDef.name} node={node} slotDef={slotDef} ui={ui} actions={actions} />
            ))}
          {extraChildNames.map((name) => (
            <ChildSlotSection key={name} node={node} ui={ui} actions={actions}
              slotDef={{
                name,
                acceptsChildList: !!(node.unknownSlotForms && node.unknownSlotForms[name] === "list"),
                objectTypes: [], textOnly: true, uncataloged: true,
              }} />
          ))}
        </div>
      )}
    </div>
  );
}

/* ----------------------------- painel de propriedades ---------------------- */

// A coluna da direita mostra só os valores de FOLHA do nó selecionado --
// desde que a árvore central passou a ser o lugar de arrastar/adicionar/
// remover/renomear componente, este painel não precisa mais duplicar
// esses controles (a antiga seção "Componentes" saiu daqui).
// Slots que o CATALOGO NAO CONHECE -- fabrica inteiramente desconhecida
// ('names' cobre TODO node.slotValues) ou classe conhecida com um slot a
// mais que o catalogo nao lista ('names' cobre so o excedente). Editavel
// como valor BRUTO (kind "raw", LeafWidget) -- sem adivinhar aspas/tipo, de
// proposito: e' o usuario quem ja sabe o que deve sair no arquivo (ver o
// comentario de scalarToSlotValue() em edl_parser_core.js pro porque disso
// importar). "+ slot" acrescenta um slot NOVO a mao -- util pra editar uma
// classe ainda em desenvolvimento, cujo catalogo nao foi regenerado ainda.
function UncatalogedSlotsSection({ node, names, onChangeSlot, onAddRawSlot }) {
  const [newName, setNewName] = useState("");
  return (
    <div className="eb-uncataloged-section">
      <div className="eb-props-subhead">slots não catalogados</div>
      {names.map((name) => (
        <div key={name} className="eb-field">
          <label className="eb-field-label">
            {name} <span className="eb-unknown-badge" title="nao catalogado -- valor bruto, sem checagem de tipo">?</span>
          </label>
          <LeafWidget slotDef={{ name }} value={node.slotValues[name]}
            onChange={(v) => onChangeSlot(node.id, name, v)} />
        </div>
      ))}
      <div className="eb-row eb-add-raw-slot">
        <input className="eb-input" placeholder="novo nome de slot" value={newName}
          onChange={(e) => setNewName(e.target.value)} />
        <button className="eb-btn eb-btn-sm" disabled={!newName.trim()}
          onClick={() => { onAddRawSlot(node.id, newName.trim()); setNewName(""); }}>
          + slot
        </button>
      </div>
    </div>
  );
}

function PropertiesPanel({ node, onChangeSlot, onAddRawSlot }) {
  if (!node) {
    return <div className="eb-pane eb-props"><p className="eb-muted eb-empty-msg">selecione um nó na árvore</p></div>;
  }
  if (node.isText) {
    return (
      <div className="eb-pane eb-props">
        <div className="eb-props-head"><div className="eb-props-title">valor de texto</div></div>
        <p className="eb-muted eb-empty-msg">edite o valor direto na árvore.</p>
      </div>
    );
  }
  const entry = CATALOG_BY_FACTORY[node.factory];
  if (!entry) {
    const names = Object.keys(node.slotValues || {});
    return (
      <div className="eb-pane eb-props">
        <div className="eb-props-head">
          <div className="eb-origin-dot eb-origin-other" />
          <div className="eb-props-title">{node.factory} <span className="eb-unknown-badge">?</span></div>
          <p className="eb-runtime-note">
            classe NÃO catalogada -- não reconhecida pelo catálogo gerado (erro de digitação,
            classe em desenvolvimento, ou plugin de terceiro nunca introspectado). Os valores
            abaixo foram preservados byte-fiéis a partir do .edl carregado; edite como texto
            bruto, exatamente como deve sair no arquivo.
          </p>
        </div>
        <UncatalogedSlotsSection node={node} names={names} onChangeSlot={onChangeSlot} onAddRawSlot={onAddRawSlot} />
      </div>
    );
  }
  const leafSlots = entry.slots.filter(isLeafSlot);
  const knownLeafNames = new Set(leafSlots.map((s) => s.name));
  const extraNames = Object.keys(node.slotValues || {}).filter((n) => !knownLeafNames.has(n));
  return (
    <div className="eb-pane eb-props">
      <div className="eb-props-head">
        <div className={`eb-origin-dot eb-origin-${originCssClass(entry.origin)}`} />
        <div className="eb-props-title">{node.factory}</div>
        {node.factory !== entry.class && <div className="eb-muted">classe C++: {entry.class}</div>}
        <div className="eb-muted">origem: {originLabel(entry.origin)}</div>
        {entry.runtimeOnly && (
          <p className="eb-runtime-note">
            classe descoberta em RUNTIME (plugin de terceiros em <code>plugins/</code>, sem fonte
            C++ neste repositório) — cadeia de herança e nomes de slot vêm de verdade, mas um slot
            marcado <span className="eb-unknown-badge">?</span> teve o TIPO adivinhado (texto ou
            número); confira contra a documentação do plugin antes de confiar no valor.
          </p>
        )}
      </div>
      {leafSlots.length === 0 && extraNames.length === 0 && <p className="eb-muted">esta classe não tem slot de valor direto.</p>}
      {leafSlots.map((slotDef) => (
        <div key={slotDef.name} className="eb-field">
          <label className="eb-field-label" title={slotDef.comment}>
            {slotDef.name}
            {slotDef.isReference && <span className="eb-ref-badge" title="referência por nome, resolvida em runtime">#</span>}
            {slotDef.typeUnknown && <span className="eb-unknown-badge" title="tipo adivinhado -- classe de terceiro sem fonte C++, confira com cuidado">?</span>}
          </label>
          <LeafWidget slotDef={slotDef} value={node.slotValues[slotDef.name]}
            onChange={(v) => onChangeSlot(node.id, slotDef.name, v)} />
        </div>
      ))}
      {extraNames.length > 0 && (
        <UncatalogedSlotsSection node={node} names={extraNames} onChangeSlot={onChangeSlot} onAddRawSlot={onAddRawSlot} />
      )}
    </div>
  );
}

/* ----------------------------------- mapa ----------------------------------- */

const MAP_PADDING_PX = 36;
const MAP_MIN_SPAN_M = 200; // evita divisao por ~0 quando todos os players coincidem (ou so' ha' um)

// SVG simples, sem dependência nova (nem canvas, nem lib de gráfico) --
// mesmo espírito de "ferramenta pequena" do resto de src/ui/. Norte pra
// CIMA, leste pra DIREITA -- a MESMA convenção da vista TopDown do mapa do
// dashboard C++ (app/MapPanel.cpp, ver CLAUDE.md), só que sem pan/zoom:
// aqui o objetivo é só "dar uma noção do cenário criado" de relance, não
// navegar um cenário grande em detalhe.
function MapPanel({ root, byFactory, selectedId, onSelect }) {
  const placements = useMemo(() => extractPlacements(root, byFactory), [root, byFactory]);
  if (!root) {
    return <div className="eb-pane eb-map"><p className="eb-muted eb-empty-msg">nada para mostrar -- crie um cenário na aba Árvore primeiro.</p></div>;
  }
  if (placements.length === 0) {
    return <div className="eb-pane eb-map"><p className="eb-muted eb-empty-msg">nenhum player (Aircraft/GuidedMissile/...) neste cenário ainda.</p></div>;
  }

  const norths = placements.map((p) => p.north);
  const easts = placements.map((p) => p.east);
  const spanN = Math.max(Math.max(...norths) - Math.min(...norths), MAP_MIN_SPAN_M);
  const spanE = Math.max(Math.max(...easts) - Math.min(...easts), MAP_MIN_SPAN_M);
  const centerN = (Math.max(...norths) + Math.min(...norths)) / 2;
  const centerE = (Math.max(...easts) + Math.min(...easts)) / 2;
  const W = 640, H = 420;
  const scale = Math.min((W - 2 * MAP_PADDING_PX) / spanE, (H - 2 * MAP_PADDING_PX) / spanN);
  const toX = (east) => W / 2 + (east - centerE) * scale;
  const toY = (north) => H / 2 - (north - centerN) * scale;

  // Grade de referência num passo "redondo" de milhas náuticas -- mesma
  // ideia de `contourIntervalFor` do mapa C++ (CLAUDE.md): escolhe o
  // primeiro passo de [0.5, 1, 2, 5, 10, 20, 50, 100, 200] NM que cobre a
  // extensão em ~6 linhas, em vez de um valor fixo que ora fica denso
  // demais (cenário grande) ora não aparece nenhuma linha (cenário pequeno).
  const NM = 1852;
  const spanNm = Math.max(spanN, spanE) / NM;
  const steps = [0.5, 1, 2, 5, 10, 20, 50, 100, 200];
  const stepNm = steps.find((s) => spanNm / s <= 6) || steps[steps.length - 1];
  const stepM = stepNm * NM;
  const gridLines = [];
  for (let k = Math.ceil((centerN - spanN) / stepM); k * stepM <= centerN + spanN; k++) {
    gridLines.push({ axis: "n", at: k * stepM });
  }
  for (let k = Math.ceil((centerE - spanE) / stepM); k * stepM <= centerE + spanE; k++) {
    gridLines.push({ axis: "e", at: k * stepM });
  }

  return (
    <div className="eb-pane eb-map">
      <div className="eb-map-head">
        <span className="eb-map-title">Mapa (topo, norte para cima)</span>
        <span className="eb-muted">{placements.length} player(es) · grade a cada {stepNm} NM</span>
      </div>
      <svg className="eb-map-svg" viewBox={`0 0 ${W} ${H}`} role="img" aria-label="mapa do cenário">
        <rect x={0} y={0} width={W} height={H} className="eb-map-bg" />
        {gridLines.map((g, i) => g.axis === "n" ? (
          <line key={`gn${i}`} x1={0} x2={W} y1={toY(g.at)} y2={toY(g.at)} className="eb-map-grid" />
        ) : (
          <line key={`ge${i}`} x1={toX(g.at)} x2={toX(g.at)} y1={0} y2={H} className="eb-map-grid" />
        ))}
        <line x1={0} x2={W} y1={toY(centerN)} y2={toY(centerN)} className="eb-map-axis" />
        <line x1={toX(centerE)} x2={toX(centerE)} y1={0} y2={H} className="eb-map-axis" />
        {placements.map((p) => {
          const cls = originCssClass(p.origin);
          const x = toX(p.east), y = toY(p.north);
          return (
            <g key={p.id} className="eb-map-marker" data-selected={p.id === selectedId ? "1" : "0"}
              onClick={() => onSelect(p.id)} style={{ cursor: "pointer" }}>
              <circle cx={x} cy={y} r={p.id === selectedId ? 9 : 6} className={`eb-map-dot eb-origin-${cls}`} />
              <text x={x + 10} y={y + 4} className="eb-map-label">{p.label}</text>
            </g>
          );
        })}
      </svg>
      <p className="eb-muted eb-map-hint">
        posição local (norte/leste, relativa ao ponto de referência da Station) lida de
        initXPos/initYPos/initAlt de cada player -- não é latitude/longitude real. Sem essas
        posições definidas, o player aparece na origem (0, 0). Clique num marcador para
        selecioná-lo na aba Árvore/no painel de propriedades.
      </p>
    </div>
  );
}

/* ------------------------------- pendências --------------------------------- */

// Descrição de UMA pendência -- reaproveitada tanto na lista quanto (se um
// dia precisar) em qualquer outro lugar que queira o mesmo texto.
function pendencyDescription(p) {
  if (p.kind === "role") return <>sem <b>{p.role}</b> (esperado: {p.baseClass})</>;
  if (p.textOnly) return <>slot <b>{p.slotName}</b> vazio (só aceita texto)</>;
  return <>slot <b>{p.slotName}</b> vazio (esperado: {(p.objectTypes && p.objectTypes.join("/")) || "componente"})</>;
}

// Aba "Pendências" -- o resumo da árvore INTEIRA, sempre visível (a
// contagem já aparece no rótulo da própria aba, sem precisar clicar) em vez
// de só o badge por cartão, que exige abrir/expandir cada um pra saber O
// QUE falta. Cada linha pula direto pro cartão na árvore (onJump), a mesma
// UX de "Ver no mapa"/seleção compartilhada já usada entre Árvore e Mapa.
function PendenciesPanel({ root, pendencies, onJump }) {
  if (!root) {
    return <div className="eb-pane eb-pending"><p className="eb-muted eb-empty-msg">nada para conferir -- crie um cenário na aba Árvore primeiro.</p></div>;
  }
  if (pendencies.length === 0) {
    return (
      <div className="eb-pane eb-pending">
        <p className="eb-ok-msg">✓ nenhuma pendência -- todo papel/slot esperado deste cenário está preenchido.</p>
      </div>
    );
  }
  return (
    <div className="eb-pane eb-pending">
      <p className="eb-muted eb-pending-summary">{pendencies.length} pendência(s) neste cenário -- clique para ir até o cartão:</p>
      <ul className="eb-pending-list">
        {pendencies.map((p, i) => (
          <li key={i} className="eb-pending-item" onClick={() => onJump(p.nodeId)}>
            <span className={`eb-origin-dot eb-origin-${originCssClass((CATALOG_BY_FACTORY[p.factory] || {}).origin)}`} />
            <span className="eb-pending-node">{p.label}</span>
            <span className="eb-pending-desc">{pendencyDescription(p)}</span>
          </li>
        ))}
      </ul>
    </div>
  );
}

/* --------------------------------- exportar -------------------------------- */

// Nome de PASTA seguro em sandbox/<nome>/ -- minúsculo, só [a-z0-9-], sem
// repetir/começar/terminar em hífen. Mesmo espírito de slug de qualquer
// gerador de nome de diretório; não precisa ser sofisticado, só não
// produzir um caminho quebrado a partir de texto livre.
function slugify(name) {
  return name.trim().toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/^-+|-+$/g, "");
}

// O caminho padrão de um cenário desta ferramenta é sandbox/<nome>/configs/
// scenario.edl -- a MESMA convenção que `./app -folder ./sandbox` já
// entende (app::discoverFolderScenarios(), ver CLAUDE.md): uma subpasta por
// cenário, com exatamente um .edl dentro de configs/. Uma página estática
// não consegue ESCREVER nesse caminho sozinha (sem servidor, sem acesso a
// disco fora do download do navegador) -- por isso o nome vira parte do
// nome sugerido do download (Chrome cria as subpastas dentro de Downloads/)
// E aparece como texto, pra quem for mover o arquivo saber exatamente onde
// ele precisa cair pra `-folder ./sandbox` enxergar o cenário.
// A File System Access API (showDirectoryPicker/getDirectoryHandle/
// getFileHandle/createWritable) está disponível em Chrome/Edge mesmo
// abrindo este .html direto por `file://` (confirmado rodando: window.
// isSecureContext e' true nos dois casos, file:// e http://localhost --
// Chromium trata file: como contexto seguro). Ausente no Firefox -- daí o
// feature-detect: com ela, "Exportar .edl" ESCREVE direto em
// sandbox/<nome>/configs/scenario.edl (a MESMA pasta que `./app -folder
// ./sandbox` já varre, sem passo manual de mover o arquivo baixado); sem
// ela, cai no download de sempre, que só pode SUGERIR esse caminho como
// nome do arquivo (Chrome/Firefox então criam a subpasta dentro de
// Downloads/, não no repositório).
const HAS_FS_ACCESS = typeof window !== "undefined" && "showDirectoryPicker" in window;

async function writeEdlIntoSandbox(sandboxDirHandle, slug, text) {
  const scenarioDir = await sandboxDirHandle.getDirectoryHandle(slug, { create: true });
  const configsDir = await scenarioDir.getDirectoryHandle("configs", { create: true });
  const fileHandle = await configsDir.getFileHandle("scenario.edl", { create: true });
  const writable = await fileHandle.createWritable();
  await writable.write(text);
  await writable.close();
}

function ExportPanel({ root }) {
  const [warn, setWarn] = useState(null);
  const [saved, setSaved] = useState(null);
  const [name, setName] = useState(() => {
    try { return localStorage.getItem("mx-edl-builder-scenario-name") || ""; } catch { return ""; }
  });
  useEffect(() => { try { localStorage.setItem("mx-edl-builder-scenario-name", name); } catch { /* sem storage -- ok */ } }, [name]);
  // O handle da pasta sandbox/ só vive NESTA aba/sessão (a API não promete
  // persistência entre recarregamentos sem um passo extra de IndexedDB, que
  // teria de re-pedir permissão de qualquer forma) -- por isso é estado
  // local, não localStorage: escolher de novo a cada sessão é o preço
  // aceitável por nunca escrever fora de onde o usuário apontou.
  const [sandboxDirHandle, setSandboxDirHandle] = useState(null);
  const text = useMemo(() => projectToEdl(root, CATALOG_BY_FACTORY), [root]);
  // Mesma gramatica de .vscode/extensions/edl/syntaxes/edl.tmLanguage.json,
  // portada em JS puro (tokenizeEdlText, edl_builder_core.js) -- so' pra
  // colorir a previa, nao muda o texto exportado em nada (o download usa
  // `text` cru, nunca os tokens).
  const tokens = useMemo(() => tokenizeEdlText(text), [text]);
  const slug = slugify(name) || "meu-cenario";
  const relPath = `sandbox/${slug}/configs/scenario.edl`;

  const chooseSandboxFolder = async () => {
    setWarn(null);
    setSaved(null);
    try {
      const handle = await window.showDirectoryPicker({ id: "mixr-edl-sandbox", mode: "readwrite" });
      setSandboxDirHandle(handle);
    } catch (err) {
      if (err && err.name !== "AbortError") {
        setWarn("Não consegui abrir o seletor de pasta: " + err.message);
      }
    }
  };

  const downloadFallback = () => {
    const blob = new Blob([text], { type: "text/plain" });
    const a = document.createElement("a");
    a.href = URL.createObjectURL(blob);
    a.download = relPath;
    a.click();
    URL.revokeObjectURL(a.href);
    setSaved(null);
  };

  const exportEdl = async () => {
    if (!isAscii(text)) {
      setWarn("Exportação BLOQUEADA: o cenário tem caractere fora de ASCII (acento, etc.). " +
        "Um único caractere assim, em qualquer lugar do arquivo, faz o parser real do MIXR " +
        "rejeitar o arquivo inteiro. Troque o texto pelo equivalente sem acento e exporte de novo.");
      return;
    }
    setWarn(null);
    if (!sandboxDirHandle) {
      downloadFallback();
      return;
    }
    try {
      // 'readwrite' já foi concedido em chooseSandboxFolder(); queryPermission
      // confirma que ainda vale (o usuário pode ter revogado pelo próprio
      // navegador entre um export e outro) sem precisar pedir de novo à toa.
      const perm = await sandboxDirHandle.queryPermission({ mode: "readwrite" });
      if (perm !== "granted") {
        const reasked = await sandboxDirHandle.requestPermission({ mode: "readwrite" });
        if (reasked !== "granted") {
          setWarn("Permissão de escrita em sandbox/ não concedida -- exportado por download em vez disso.");
          downloadFallback();
          return;
        }
      }
      await writeEdlIntoSandbox(sandboxDirHandle, slug, text);
      setSaved(relPath);
    } catch (err) {
      setWarn("Falha ao salvar em " + relPath + ": " + err.message + " -- exportado por download em vez disso.");
      downloadFallback();
    }
  };

  return (
    <div className="eb-pane eb-export">
      <div className="eb-export-head">
        <span className="eb-export-title">⤓ Pré-visualização .edl</span>
        <span className="eb-row">
          {HAS_FS_ACCESS && (
            <button className="eb-btn eb-btn-sm" onClick={chooseSandboxFolder}
              title="Escolher a pasta sandbox/ deste repositório -- uma vez por sessão">
              {sandboxDirHandle ? "✓ pasta sandbox/ escolhida" : "Escolher pasta sandbox/…"}
            </button>
          )}
          <button className="eb-btn" disabled={!root} onClick={exportEdl}>
            {sandboxDirHandle ? "Salvar em sandbox/" : "Exportar .edl"}
          </button>
        </span>
      </div>
      <div className="eb-export-path">
        <label className="eb-field-label" htmlFor="eb-scenario-name">nome do cenário (pasta em <code>sandbox/</code>)</label>
        <input id="eb-scenario-name" className="eb-input" type="text" value={name} placeholder="meu-cenario"
          onChange={(e) => setName(e.target.value)} />
        <p className="eb-muted eb-export-path-hint">
          {sandboxDirHandle
            ? <>salva DIRETO em <code>{relPath}</code> (dentro da pasta escolhida) -- pronto para <code>./app -folder ./sandbox</code>.</>
            : HAS_FS_ACCESS
              ? <>baixa como <code>./{relPath}</code> (Downloads/); clique em “Escolher pasta sandbox/…” acima para salvar direto no repositório.</>
              : <>baixa como <code>./{relPath}</code>.</>}
        </p>
      </div>
      {warn && <p className="eb-warn">{warn}</p>}
      {saved && !warn && <p className="eb-ok-msg">✓ salvo em <code>{saved}</code></p>}
      <pre className="eb-edl-preview">
        {text
          ? tokens.map((t, i) => (t.cls ? <span key={i} className={t.cls}>{t.text}</span> : t.text))
          : "(cenário vazio -- arraste uma classe para começar)"}
      </pre>
    </div>
  );
}

/* -------------------------- avisos de carregamento -------------------------- */
// Faixa dispensável, logo abaixo da barra de ferramentas, só quando "Carregar
// .edl" termina com avisos (não bloqueantes -- nada foi perdido, ver
// edl_parser_core.js). Mesmo visual de `.eb-pending-item`/`.eb-pending-list`
// (Pendências) e o MESMO mecanismo de "pular até o nó" (handleJumpToPendency,
// que já expande ancestrais/seleciona/troca pra aba Árvore -- genérico o
// bastante pra servir aqui também, sem duplicar lógica).
function LoadWarningsBanner({ warnings, onJump, onDismiss }) {
  if (!warnings || warnings.length === 0) return null;
  return (
    <div className="eb-load-warnings">
      <div className="eb-load-warnings-head">
        <span>{warnings.length} aviso(s) ao carregar este .edl -- nada foi perdido, mas confira:</span>
        <button className="eb-btn eb-btn-sm" onClick={onDismiss}>dispensar</button>
      </div>
      <ul className="eb-pending-list">
        {warnings.map((w, i) => (
          <li key={i} className="eb-pending-item" style={{ cursor: w.nodeId != null ? "pointer" : "default" }}
            onClick={() => w.nodeId != null && onJump(w.nodeId)}>
            <span className="eb-pending-desc">{w.message}</span>
          </li>
        ))}
      </ul>
    </div>
  );
}

/* ----------------------------------- App ----------------------------------- */

const CSS = `
.eb { --paper:#E6E9E3; --panel:#DCE0D9; --ink:#16232E; --muted:#6E7A76;
  --rule:#C6CDC3; --hot:#B4661E; --ok:#4A6B4F; --bad:#9C3B3B;
  --frame-bg: rgba(0,0,0,0.035); --placeholder-bg: rgba(0,0,0,0.02); --branch:#8A968D;
  --syn-comment:#7C8A82; --syn-string:#4A7A5E; --syn-class:#2C4B7C; --syn-slot:#8A5A1E;
  --syn-bool:#B4661E; --syn-num:#6B4FA0;
  --mono: ui-monospace,'JetBrains Mono','SF Mono',Menlo,monospace;
  --sans: 'Inter',system-ui,-apple-system,sans-serif;
  background:var(--paper); color:var(--ink); font-family:var(--sans);
  font-size:13.5px; line-height:1.5; height:100vh; color-scheme: light;
  display:flex; flex-direction:column; overflow:hidden; box-sizing:border-box;
  --origin-models-bg:#E3EBF7; --origin-models-fg:#2C4B7C;
  --origin-base-bg:#EAEBE6; --origin-base-fg:#52564E;
  --origin-terrain-bg:#E7EEDD; --origin-terrain-fg:#47623A;
  --origin-dis-bg:#EFE6F5; --origin-dis-fg:#5B3E75;
  --origin-linkage-bg:#E1F0EE; --origin-linkage-fg:#2B6F66;
  --origin-recorder-bg:#FBEEDA; --origin-recorder-fg:#8A5A1E;
  --origin-simulation-bg:#E7E6F6; --origin-simulation-fg:#453C82;
  --origin-libs-bg:#FBE7ED; --origin-libs-fg:#8C3B58;
  --origin-plugin-bg:#FCEADB; --origin-plugin-fg:#8A4B1B;
  --origin-other-bg:var(--panel); --origin-other-fg:var(--muted); }
.eb[data-theme="dark"] { --paper:#181C19; --panel:#232722; --ink:#E7EAE4; --muted:#8B968E;
  --rule:#3A413B; --hot:#D98A4A; --ok:#7FBE8B; --bad:#E08A8A; color-scheme: dark;
  --frame-bg: rgba(255,255,255,0.04); --placeholder-bg: rgba(255,255,255,0.025); --branch:#6E7A73;
  --syn-comment:#7E8983; --syn-string:#8FCB9E; --syn-class:#9DBBE3; --syn-slot:#E8C284;
  --syn-bool:#D98A4A; --syn-num:#C3ADEA;
  --origin-models-bg:#1E2A3C; --origin-models-fg:#9DBBE3;
  --origin-base-bg:#2A2C27; --origin-base-fg:#C7CAC0;
  --origin-terrain-bg:#223420; --origin-terrain-fg:#A9CC9A;
  --origin-dis-bg:#2E2438; --origin-dis-fg:#D2B8E8;
  --origin-linkage-bg:#1B2E2B; --origin-linkage-fg:#8FD9CC;
  --origin-recorder-bg:#362A16; --origin-recorder-fg:#E8C284;
  --origin-simulation-bg:#241F3A; --origin-simulation-fg:#BDB3EA;
  --origin-libs-bg:#351E27; --origin-libs-fg:#E8A9C0;
  --origin-plugin-bg:#362619; --origin-plugin-fg:#E8B583;
  --origin-other-bg:var(--panel); --origin-other-fg:var(--muted); }
.eb-bar { flex-shrink:0; background:var(--paper); border-bottom:1px solid var(--rule);
  padding:10px 18px 8px; display:flex; justify-content:space-between; align-items:center; gap:14px; flex-wrap:wrap; }
.eb-h1 { font-size:16px; font-weight:600; margin:0; letter-spacing:-0.01em; }
.eb-sub { font-size:12px; color:var(--muted); margin:2px 0 0; }
.eb-btn { font:inherit; font-size:12.5px; padding:5px 12px; cursor:pointer;
  border:1px solid var(--rule); background:transparent; color:var(--ink); border-radius:2px; }
.eb-btn:hover:not(:disabled) { border-color:var(--ink); }
.eb-btn:disabled { opacity:0.5; cursor:default; }
.eb-btn-sm { font-size:11px; padding:2px 8px; }
/* Shell de app: bar (altura fixa) + body (o resto), body em TRES colunas
   flex, cada uma com altura=100% do body e rolagem PROPRIA -- ao contrario
   do desenho anterior (grid + position:sticky), nenhuma coluna pode
   "vazar" pra baixo do fim da tela nem sobrepor a coluna vizinha: a altura
   de cada uma e' travada pelo flexbox, nao aproximada por calc(100vh-Npx). */
.eb-body { flex:1; min-height:0; display:flex; flex-direction:row; gap:12px;
  padding:12px 18px 24px; box-sizing:border-box; overflow:hidden; }
.eb-pane { border:1px solid var(--rule); border-radius:2px; background:var(--panel); padding:10px; box-sizing:border-box; }
.eb-palette { width:300px; flex-shrink:0; height:100%; overflow-y:auto; overflow-x:hidden; }
.eb-palette-list { margin-top:8px; }
.eb-group-title { font-weight:600; font-size:11.5px; text-transform:uppercase; letter-spacing:.03em;
  color:var(--muted); margin:10px 0 4px; }
.eb-chip { font-family:var(--mono); font-size:12px; padding:3px 6px; margin:2px 0; border:1px solid var(--rule);
  border-radius:2px; background:var(--paper); cursor:grab; border-left:3px solid var(--origin-other-fg);
  display:block; white-space:nowrap; overflow:hidden; text-overflow:ellipsis; }
.eb-chip:hover { border-color:var(--hot); }
.eb-alias { color:var(--muted); font-style:normal; font-size:10.5px; }
/* .eb-main: a coluna do meio -- arvore + previa .edl empilhadas, rolando
   JUNTAS numa unica barra de rolagem propria (nunca mais um "rodape" que
   se estende por baixo da paleta: a previa mora DENTRO desta coluna,
   nunca mais grid-column:1/-1). */
.eb-main { flex:1; min-width:0; height:100%; overflow-y:auto; display:flex; flex-direction:column; gap:12px; }
.eb-main-tabs { flex-shrink:0; }
.eb-tree-utility-row { margin-left:auto; }
.eb-tab-btn { font:inherit; font-size:12.5px; padding:5px 14px; cursor:pointer;
  border:1px solid var(--rule); border-bottom:none; background:var(--paper); color:var(--muted); border-radius:2px 2px 0 0; }
.eb-tab-btn-active { color:var(--ink); font-weight:600; background:var(--panel); border-color:var(--ink); }
.eb-tab-btn-pending { color:var(--hot); }
.eb-tab-btn-pending.eb-tab-btn-clear { color:var(--ok); }
.eb-tab-btn-pending.eb-tab-btn-active { color:var(--ink); }
.eb-uncataloged-count { font-size:10.5px; padding:1px 6px; border-radius:8px; border:1px dashed var(--muted);
  color:var(--muted); align-self:center; }
.eb-pending { min-height:200px; flex-shrink:0; }
.eb-pending-summary { margin:0 0 8px; }
.eb-pending-list { list-style:none; margin:0; padding:0; display:flex; flex-direction:column; gap:2px; }
.eb-pending-item { display:flex; align-items:center; gap:8px; padding:6px 8px; border-radius:2px;
  background:var(--paper); border:1px solid var(--rule); cursor:pointer; font-size:12.5px; }
.eb-pending-item:hover { border-color:var(--hot); }
.eb-pending-node { font-family:var(--mono); font-weight:600; flex-shrink:0; }
.eb-pending-desc { color:var(--muted); }
/* Faixa dispensavel de avisos de "Carregar .edl" -- mesma lista de
   .eb-pending-item/.eb-pending-list acima, so' que fora das abas (aparece
   logo abaixo da barra de ferramentas, nao dentro do corpo). */
.eb-load-warnings { flex-shrink:0; margin:0 18px 0; padding:8px 12px; border:1px solid var(--hot);
  border-radius:2px; background:var(--placeholder-bg); }
.eb-load-warnings-head { display:flex; justify-content:space-between; align-items:center;
  font-size:12.5px; font-weight:600; margin-bottom:6px; }
.eb-tree { min-height: 200px; flex-shrink:0; }
.eb-map { min-height: 200px; flex-shrink:0; }
.eb-map-head { display:flex; justify-content:space-between; align-items:baseline; margin-bottom:8px; }
.eb-map-title { font-weight:600; font-size:13px; }
.eb-map-svg { width:100%; height:auto; border:1px solid var(--rule); border-radius:2px; background:var(--paper); }
.eb-map-bg { fill:var(--paper); }
.eb-map-grid { stroke:var(--rule); stroke-width:1; }
.eb-map-axis { stroke:var(--muted); stroke-width:1; stroke-dasharray:3 3; }
.eb-map-dot { stroke:var(--paper); stroke-width:1.5; }
.eb-map-marker[data-selected="1"] .eb-map-dot { stroke:var(--hot); stroke-width:2.5; }
.eb-map-marker:hover .eb-map-dot { opacity:0.8; }
.eb-map-label { font-family:var(--mono); font-size:10.5px; fill:var(--ink); user-select:none; }
.eb-map-hint { font-size:11.5px; margin-top:8px; }
.eb-props { width:320px; flex-shrink:0; height:100%; overflow-y:auto; }
.eb-props-head { margin-bottom: 8px; }
.eb-props-title { font-weight:600; font-family:var(--mono); }
.eb-field { margin:9px 0; }
.eb-field-label { display:block; font-size:12px; color:var(--muted); margin-bottom:2px; }
.eb-ref-badge { color:var(--hot); margin-left:4px; }
.eb-unknown-badge { color:var(--bad); margin-left:4px; font-weight:700; cursor:help; }
.eb-runtime-note { font-size:11px; color:var(--muted); background:var(--placeholder-bg);
  border:1px dashed var(--rule); border-radius:4px; padding:6px 8px; margin-top:6px; }
.eb-runtime-note code { font-family:var(--mono); }
.eb-chip-runtime { border-style:dashed; }
/* Slots/fabricas que o catalogo nao conhece (carregados de um .edl real, ou
   acrescentados a mao via "+ slot") -- mesma familia visual de
   .eb-runtime-note/.eb-unknown-badge, so' com a borda tracejada tambem no
   CARTAO inteiro quando a fabrica em si e' desconhecida (severidade maior
   que "so um campo com tipo adivinhado"). */
.eb-card-uncataloged { border-style:dashed; border-color:var(--hot); }
.eb-slot-frame-uncataloged { border:1px dashed var(--hot); }
.eb-uncataloged-section { background:var(--frame-bg); border-radius:6px; padding:7px 8px; margin:10px 0 0; }
.eb-props-subhead { font-weight:600; font-size:10.5px; text-transform:uppercase; letter-spacing:.03em;
  color:var(--muted); margin-bottom:6px; }
.eb-add-raw-slot { margin-top:8px; }
.eb-add-raw-slot .eb-input { width:auto; flex:1; }
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
.eb-key { font-family:var(--mono); font-size:11.5px; width:70px; padding:2px 4px; border:1px solid var(--rule); border-radius:2px; }
.eb-x { margin-left:auto; border:none; background:transparent; color:var(--bad); cursor:pointer; font-size:14px; line-height:1; }
.eb-dropzone.eb-drop-ok, .eb-placeholder.eb-drop-ok { border-color:var(--ok) !important; color:var(--ok); }
.eb-dropzone.eb-drop-bad, .eb-placeholder.eb-drop-bad { border-color:var(--bad) !important; opacity:0.6; }
/* Visualmente DISTINTO da árvore acima -- borda superior grossa + fundo
   trocado (paper no lugar de panel) + título maiúsculo, pra não parecer
   "mais um cartão dentro da árvore" (era a confusão relatada): isto é uma
   ferramenta à parte (exportar), não um nó do cenário. */
.eb-export { flex-shrink:0; background:var(--paper); border-top:3px solid var(--hot); }
.eb-export-head { display:flex; justify-content:space-between; align-items:center; }
.eb-export-title { font-weight:700; font-size:13px; text-transform:uppercase; letter-spacing:.04em; }
.eb-export-path { margin-top:8px; max-width:480px; }
.eb-export-path input { margin-top:2px; }
.eb-export-path-hint { margin:4px 0 0; }
.eb-export-path-hint code, .eb-field-label code { font-family:var(--mono); background:var(--paper);
  border:1px solid var(--rule); border-radius:2px; padding:0 3px; }
.eb-edl-preview { font-family:var(--mono); font-size:11.5px; background:var(--paper); border:1px solid var(--rule);
  border-radius:2px; padding:10px; max-height:280px; overflow:auto; white-space:pre; margin-top:8px; }
.eb-warn { color:var(--bad); font-size:12.5px; }
.eb-ok-msg { color:var(--ok); font-size:12.5px; }
/* Mesmas categorias de .vscode/extensions/edl/syntaxes/edl.tmLanguage.json
   (ver tokenizeEdlText, edl_builder_core.js) -- so' cor, sem negrito extra
   pesado (a fonte mono ja' e' pequena o bastante sem precisar competir). */
.tok-comment { color:var(--syn-comment); font-style:italic; }
.tok-string { color:var(--syn-string); }
.tok-class { color:var(--syn-class); font-weight:600; }
.tok-slot { color:var(--syn-slot); }
.tok-bool { color:var(--syn-bool); }
.tok-num { color:var(--syn-num); }
.tok-punct { color:var(--muted); }
.tok-value { color:var(--ink); }

/* --------------------------- árvore de cartões --------------------------- */
.eb-card { border:1px solid var(--rule); border-radius:8px; margin:6px 0; overflow:hidden; }
.eb-card-selected { border-color:var(--hot); box-shadow: 0 0 0 1px var(--hot); }
.eb-card-head { display:flex; align-items:center; gap:6px; padding:6px 8px; cursor:pointer;
  font-family:var(--mono); font-size:12.5px; background:var(--origin-other-bg); }
.eb-card.eb-origin-models > .eb-card-head { background:var(--origin-models-bg); }
.eb-card.eb-origin-base > .eb-card-head { background:var(--origin-base-bg); }
.eb-card.eb-origin-terrain > .eb-card-head { background:var(--origin-terrain-bg); }
.eb-card.eb-origin-dis > .eb-card-head { background:var(--origin-dis-bg); }
.eb-card.eb-origin-linkage > .eb-card-head { background:var(--origin-linkage-bg); }
.eb-card.eb-origin-recorder > .eb-card-head { background:var(--origin-recorder-bg); }
.eb-card.eb-origin-simulation > .eb-card-head { background:var(--origin-simulation-bg); }
.eb-card.eb-origin-libs > .eb-card-head { background:var(--origin-libs-bg); }
.eb-card.eb-origin-plugin > .eb-card-head { background:var(--origin-plugin-bg); }
.eb-chevron { border:none; background:transparent; cursor:pointer; color:var(--muted); font-size:11px;
  width:16px; padding:0; line-height:1; }
.eb-chevron:hover { color:var(--ink); }
.eb-chevron-spacer { display:inline-block; width:16px; }
.eb-origin-dot { display:inline-block; width:9px; height:9px; border-radius:50%; flex-shrink:0; }
.eb-origin-dot.eb-origin-models { background:var(--origin-models-fg); }
.eb-origin-dot.eb-origin-base { background:var(--origin-base-fg); }
.eb-origin-dot.eb-origin-terrain { background:var(--origin-terrain-fg); }
.eb-origin-dot.eb-origin-dis { background:var(--origin-dis-fg); }
.eb-origin-dot.eb-origin-linkage { background:var(--origin-linkage-fg); }
.eb-origin-dot.eb-origin-recorder { background:var(--origin-recorder-fg); }
.eb-origin-dot.eb-origin-simulation { background:var(--origin-simulation-fg); }
.eb-origin-dot.eb-origin-libs { background:var(--origin-libs-fg); }
.eb-origin-dot.eb-origin-plugin { background:var(--origin-plugin-fg); }
.eb-origin-dot.eb-origin-other { background:var(--origin-other-fg); }
/* Mesma paleta por origem, aplicada a marcador de SVG (fill, nao background). */
.eb-map-dot.eb-origin-models { fill:var(--origin-models-fg); }
.eb-map-dot.eb-origin-base { fill:var(--origin-base-fg); }
.eb-map-dot.eb-origin-terrain { fill:var(--origin-terrain-fg); }
.eb-map-dot.eb-origin-dis { fill:var(--origin-dis-fg); }
.eb-map-dot.eb-origin-linkage { fill:var(--origin-linkage-fg); }
.eb-map-dot.eb-origin-recorder { fill:var(--origin-recorder-fg); }
.eb-map-dot.eb-origin-simulation { fill:var(--origin-simulation-fg); }
.eb-map-dot.eb-origin-libs { fill:var(--origin-libs-fg); }
.eb-map-dot.eb-origin-plugin { fill:var(--origin-plugin-fg); }
.eb-map-dot.eb-origin-other { fill:var(--origin-other-fg); }
.eb-node-factory { font-weight:600; }
.eb-fill-count { color:var(--muted); font-size:11px; }
.eb-missing-badge { font-size:10.5px; padding:1px 6px; border-radius:8px; border:1px dashed var(--hot);
  color:var(--hot); background:transparent; }
.eb-card-body { padding:6px 10px 8px 22px; }
.eb-role-frame, .eb-slot-frame { background:var(--frame-bg); border-radius:6px; padding:7px 8px; margin:6px 0; }
.eb-role-frame-title, .eb-slot-frame-title { font-weight:600; font-size:10.5px; text-transform:uppercase;
  letter-spacing:.03em; color:var(--muted); margin-bottom:5px; }
.eb-role-row { display:flex; align-items:flex-start; gap:8px; }
.eb-role-label { font-family:var(--mono); font-size:11px; color:var(--muted); width:118px; flex-shrink:0;
  padding-top:6px; }
.eb-role-row > .eb-card, .eb-role-row > .eb-placeholder { flex:1; margin:0; }
.eb-role-others { margin-top:6px; padding-top:6px; border-top:1px dashed var(--rule); }

/* -------------------------- conectores de árvore --------------------------- */
/* Trilho vertical + galho horizontal por item -- o que faz a hierarquia LER
   como árvore (tronco+galho, estilo comando tree/explorador de arquivos),
   não só caixas aninhadas com recuo. Cada .eb-branch-item é um "nó" da lista; o
   ÚLTIMO item corta o trilho vertical no próprio galho (::after com
   height fixo em vez de esticar até o próximo -- senão a linha "sobraria"
   pendurada abaixo do último item, sem nada pra conectar). */
.eb-branch-list { display:flex; flex-direction:column; gap:6px; margin-left:7px; }
.eb-branch-item { position:relative; padding-left:18px; }
.eb-branch-item::before {
  content:""; position:absolute; left:0; top:15px; width:16px; height:2px; background:var(--branch); border-radius:1px; }
.eb-branch-item::after {
  content:""; position:absolute; left:0; top:0; bottom:-6px; width:2px; background:var(--branch); }
.eb-branch-item:last-child::after { bottom:auto; height:16px; }
.eb-placeholder { display:flex; align-items:center; gap:6px; flex-wrap:wrap; font-size:11.5px; color:var(--muted);
  border:1px dashed var(--rule); border-radius:6px; padding:6px 8px; margin:4px 0; background:var(--placeholder-bg);
  font-style:italic; }
.eb-placeholder-compact { padding:3px 8px; }
.eb-placeholder-label { flex-shrink:0; }
.eb-tree-textitem { display:flex; align-items:center; gap:6px; padding:4px 6px; margin:4px 0;
  border:1px solid var(--rule); border-radius:6px; background:var(--paper); }
.eb-tree-textitem .eb-input { flex:1; width:auto; }
`;

// A ferramenta abre com a árvore VAZIA por padrão -- ver o "porquê" na
// seção correspondente de src/ui/README.md (decisão explícita: carregar
// automaticamente um cenário grande de exemplo escondia com que se estava
// mexendo de fato). EDL_PRESET é injetado por compile.js (mesmo mecanismo
// de EDL_CATALOG) a partir de src/ui/preset.json -- gerado por
// src/ui/scripts/build.js contra src/poc/built-in_mixr_1/configs/
// scenario_max_player.edl.in, o cenário com mais componentes do
// repositório (53 das 96 classes de mixr::models num Aircraft só) -- fica
// disponível SOB DEMANDA, pelo botão "Carregar preset", nunca mais
// carregado sozinho.
function loadPresetTree() {
  if (typeof EDL_PRESET === "undefined" || !EDL_PRESET) return null;
  resetIdCounter(1 + maxId(EDL_PRESET, 0));
  return EDL_PRESET;
}

export default function App() {
  const [root, setRoot] = useState(null);
  const [selectedId, setSelectedId] = useState(null);
  const [dragFactory, setDragFactory] = useState(null);
  const [expandedIds, setExpandedIds] = useState(() => new Set());
  const [mainTab, setMainTab] = useState("tree");
  const [theme, setTheme] = useState(() => {
    try { return localStorage.getItem("mx-edl-builder-theme") === "dark" ? "dark" : "light"; } catch { return "light"; }
  });
  useEffect(() => { try { localStorage.setItem("mx-edl-builder-theme", theme); } catch { /* sem storage -- ok */ } }, [theme]);

  const selectedNode = useMemo(() => findNode(root, selectedId), [root, selectedId]);

  const toggleExpand = useCallback((id) => {
    setExpandedIds((prev) => {
      const next = new Set(prev);
      if (next.has(id)) next.delete(id); else next.add(id);
      return next;
    });
  }, []);

  // "Expandir tudo"/"Recolher tudo" -- utilitários de árvore grande (o
  // preset de built-in_mixr_1 tem ~193 nós): reaproveita defaultExpandedIds
  // com profundidade Infinity (todo nó entra) em vez de duplicar a
  // travessia. A raiz já é sempre expandida independente de expandedIds
  // (ver TreeNode: `isRoot || ui.expandedIds.has(...)`), então "recolher
  // tudo" some com os FILHOS, não com o cenário inteiro.
  const handleExpandAll = useCallback(() => {
    setExpandedIds(defaultExpandedIds(root, Infinity));
  }, [root]);
  const handleCollapseAll = useCallback(() => {
    setExpandedIds(new Set());
  }, []);

  // Aba "Pendências" -- recalculada a cada mudança de árvore (percorrer
  // ~200 nós é barato; não vale a pena memoizar por nó, só pela árvore
  // inteira). handleJumpToPendency troca pra aba Árvore, seleciona o nó e
  // expande todo ancestral no caminho até ele (sem isso o cartão-alvo
  // poderia estar escondido atrás de um pai colapsado).
  const pendencies = useMemo(() => collectPendencies(root, CATALOG_BY_FACTORY), [root]);
  const handleJumpToPendency = useCallback((nodeId) => {
    setMainTab("tree");
    setSelectedId(nodeId);
    const path = findAncestorPath(root, nodeId);
    if (path) {
      setExpandedIds((prev) => {
        const next = new Set(prev);
        path.forEach((id) => next.add(id));
        return next;
      });
    }
  }, [root]);

  // suggestedKey: usado só pelos placeholders de PAPEL (RoleSection), pra
  // o item recem-criado nascer com a chave convencional ('dynamicsModel',
  // 'pilot', ...) em vez de um numero -- cosmetico (o MIXR resolve por
  // TIPO, nao pela chave), mas deixa o .edl exportado parecido com os
  // cenarios de producao. Cai pro numero auto-incrementado de sempre se a
  // chave sugerida ja estiver em uso.
  const handleDrop = useCallback((nodeId, slotName, factory, suggestedKey) => {
    setRoot((r) => updateNode(r, nodeId, (n) => {
      const slotDef = CATALOG_BY_FACTORY[n.factory].slots.find((s) => s.name === slotName);
      const child = makeNode(factory);
      const existing = n.children[slotName] || [];
      let key;
      if (slotDef.acceptsChildList) {
        key = suggestedKey && !existing.some((it) => it.key === suggestedKey)
          ? suggestedKey
          : String(existing.length + 1);
      } else {
        key = "1";
      }
      const items = slotDef.acceptsChildList ? [...existing, { key, node: child }] : [{ key, node: child }];
      return { ...n, children: { ...n.children, [slotName]: items } };
    }));
    setExpandedIds((prev) => new Set(prev).add(nodeId));
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

  // Acrescenta um slot NOVO, a mao, com valor bruto vazio -- a UI de
  // UncatalogedSlotsSection (PropertiesPanel) usa isto tanto pra uma
  // classe inteiramente desconhecida quanto pra um slot a mais numa classe
  // conhecida (util editando uma classe ainda em desenvolvimento, cujo
  // catalogo nao foi regenerado ainda).
  const handleAddRawSlot = useCallback((nodeId, slotName) => {
    setRoot((r) => updateNode(r, nodeId, (n) => ({
      ...n, slotValues: { ...n.slotValues, [slotName]: { kind: "raw", raw: "" } },
    })));
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
    setRoot(null); setSelectedId(null); setExpandedIds(new Set());
  };

  // "Excluir a classe raiz" -- descarta a árvore inteira (não há Station
  // "vazia": a raiz É o cenário). Mesma confirmação de "Novo", só que
  // disparada a partir do próprio nó raiz, não da barra de ferramentas --
  // mais descobrível do que só o botão "Novo" genérico.
  const handleRemoveRoot = handleNew;

  // Carrega o preset embutido (EDL_PRESET, ver `loadPresetTree()` acima --
  // src/ui/preset.json, gerado a partir do cenário "player máximo" de
  // built-in_mixr_1) por cima da árvore atual. Mesma confirmação de
  // "Novo"/handleRemoveRoot -- é descarte do trabalho em andamento, não um
  // merge. `resetIdCounter`/`defaultExpandedIds` seguem o MESMO trio de
  // sempre (compartilhado com `handleOpenEdl()`, mais abaixo): o preset é
  // uma árvore de projeto como outra qualquer, só que embutida no .html em
  // vez de carregada de um arquivo.
  const handleLoadPreset = () => {
    if (root && !window.confirm("Descartar o cenário atual e carregar o preset?")) return;
    const preset = loadPresetTree();
    if (!preset) {
      window.alert("Nenhum preset embutido nesta build (src/ui/preset.json não foi gerado).");
      return;
    }
    setRoot(preset);
    setSelectedId(null);
    setExpandedIds(defaultExpandedIds(preset, 2));
  };

  // "Carregar .edl" -- abre um .edl/.edl.in REAL qualquer e mapeia tudo pra
  // esta mesma árvore, via o motor compartilhado de
  // src/ui/edl_parser_core.js (concatenado por compile.js na mesma
  // <script>, sem import nenhum -- mesmo mecanismo de sempre). Erro DURO
  // (parêntese sem fechamento, '@include:frag@' -- não suportado por este
  // caminho, ver o comentário de parseEdlDocument() em edl_parser_core.js)
  // mostra a mensagem e não toca a árvore atual. Sucesso pede confirmação
  // de descarte só se já houver trabalho em andamento (mesmo guard de
  // handleLoadPreset), depois segue o TRIO de sempre
  // (resetIdCounter/setRoot/setExpandedIds) -- e guarda os avisos não-
  // bloqueantes (fábrica/slot não catalogado, placeholder '@TOKEN@' ainda
  // literal, etc.) pra faixa dispensável logo abaixo da barra.
  const edlFileInputRef = useRef(null);
  const [loadWarnings, setLoadWarnings] = useState([]);
  const handleOpenEdl = (file) => {
    const reader = new FileReader();
    reader.onload = () => {
      const text = String(reader.result);
      const { tree, warnings, errors } = parseEdlDocument(text, CATALOG_BY_FACTORY);
      if (errors.length) {
        window.alert("Não consegui carregar este .edl:\n\n" + errors.map((e) => e.message).join("\n"));
        return;
      }
      if (root && !window.confirm("Descartar o cenário atual e carregar este .edl?")) return;
      resetIdCounter(1 + maxId(tree, 0));
      setRoot(tree);
      setSelectedId(null);
      setExpandedIds(defaultExpandedIds(tree, 2));
      setLoadWarnings(warnings);
    };
    reader.onerror = () => window.alert("Não consegui ler o arquivo.");
    reader.readAsText(file);
  };

  // Badge persistente da barra de abas -- continua visível mesmo depois da
  // faixa de avisos acima ser dispensada, pra "tem conteúdo não catalogado
  // neste cenário" nunca desaparecer de vista (countUncataloged conta NÓS,
  // não slots -- um Aircraft com três slots desconhecidos conta 1, ver
  // edl_builder_core.js).
  const uncatalogedCount = useMemo(() => countUncataloged(root, CATALOG_BY_FACTORY), [root]);

  const ui = { byFactory: CATALOG_BY_FACTORY, dragFactory, selectedId, expandedIds };
  const actions = {
    onSelect: setSelectedId, onToggleExpand: toggleExpand, onDrop: handleDrop,
    onRemoveItem: handleRemoveItem, onRenameKey: handleRenameKey,
    onAddText: handleAddText, onChangeText: handleChangeText,
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
          <button className="eb-btn" onClick={() => edlFileInputRef.current && edlFileInputRef.current.click()}
            title="Carrega um .edl/.edl.in REAL qualquer e mapeia tudo pros componentes desta ferramenta">
            Carregar .edl
          </button>
          <input ref={edlFileInputRef} type="file" accept=".edl,.edl.in,text/plain" style={{ display: "none" }}
            onChange={(e) => { if (e.target.files[0]) handleOpenEdl(e.target.files[0]); e.target.value = ""; }} />
          <button className="eb-btn" onClick={handleLoadPreset} title="Carrega o cenário de exemplo (player máximo, built-in_mixr_1)">
            Carregar preset
          </button>
          <button className="eb-btn" onClick={() => setTheme((t) => (t === "light" ? "dark" : "light"))}>
            {theme === "light" ? "☾ escuro" : "☀ claro"}
          </button>
        </div>
      </div>
      <LoadWarningsBanner warnings={loadWarnings} onJump={handleJumpToPendency} onDismiss={() => setLoadWarnings([])} />
      <div className="eb-body">
        <Palette onDragStartFactory={setDragFactory} />
        <div className="eb-main">
          <div className="eb-row eb-main-tabs">
            <button className={`eb-tab-btn${mainTab === "tree" ? " eb-tab-btn-active" : ""}`} onClick={() => setMainTab("tree")}>Árvore</button>
            <button className={`eb-tab-btn${mainTab === "map" ? " eb-tab-btn-active" : ""}`} onClick={() => setMainTab("map")}>Mapa</button>
            <button className={`eb-tab-btn eb-tab-btn-pending${pendencies.length === 0 ? " eb-tab-btn-clear" : ""}${mainTab === "pending" ? " eb-tab-btn-active" : ""}`}
              onClick={() => setMainTab("pending")}>
              Pendências {pendencies.length === 0 ? "✓" : `(${pendencies.length})`}
            </button>
            {uncatalogedCount > 0 && (
              <span className="eb-uncataloged-count"
                title="componentes com fábrica ou slot que o catálogo não conhece -- valores preservados como texto bruto, editáveis no painel de propriedades">
                {uncatalogedCount} não catalogado{uncatalogedCount > 1 ? "s" : ""}
              </span>
            )}
            {mainTab === "tree" && root && (
              <span className="eb-row eb-tree-utility-row">
                <button className="eb-btn eb-btn-sm" onClick={handleExpandAll} title="Expande todo nó da árvore">Expandir tudo</button>
                <button className="eb-btn eb-btn-sm" onClick={handleCollapseAll} title="Recolhe todo nó (a raiz continua visível)">Recolher tudo</button>
              </span>
            )}
          </div>
          {mainTab === "tree" ? (
            <div className="eb-pane eb-tree">
              {!root ? (
                <div className="eb-root-drop" data-active={dragFactory ? "1" : "0"}
                  onDragOver={(e) => { if (dragFactory && isCompatible(dragFactory, ROOT_SLOT_DEF, CATALOG_BY_FACTORY)) e.preventDefault(); }}
                  onDrop={(e) => {
                    e.preventDefault();
                    const factory = e.dataTransfer.getData("text/plain");
                    if (factory && isCompatible(factory, ROOT_SLOT_DEF, CATALOG_BY_FACTORY)) {
                      const node = makeNode(factory);
                      setRoot(node);
                      setExpandedIds(new Set([node.id]));
                    }
                  }}>
                  arraste uma classe que implemente Station aqui para começar (ex.: Station ou ClockStation)
                  <div style={{ marginTop: 10 }}>
                    <AddViaSelect slotDef={ROOT_SLOT_DEF} onPick={(f) => { const n = makeNode(f); setRoot(n); setExpandedIds(new Set([n.id])); }} />
                  </div>
                  <p className="eb-muted eb-empty-msg" style={{ marginTop: 14 }}>
                    ou clique em “Carregar preset”, no topo, para abrir um cenário de exemplo já pronto
                  </p>
                </div>
              ) : (
                <TreeNode node={root} ui={ui} actions={actions} isRoot onRemoveLocal={handleRemoveRoot} />
              )}
            </div>
          ) : mainTab === "map" ? (
            <MapPanel root={root} byFactory={CATALOG_BY_FACTORY} selectedId={selectedId} onSelect={setSelectedId} />
          ) : (
            <PendenciesPanel root={root} pendencies={pendencies} onJump={handleJumpToPendency} />
          )}
          <ExportPanel root={root} />
        </div>
        <PropertiesPanel node={selectedNode} onChangeSlot={handleChangeSlot} onAddRawSlot={handleAddRawSlot} />
      </div>
    </div>
  );
}
