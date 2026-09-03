import { useEffect, useState } from 'react';
import { api } from './api/client';
import { parseScenario, EdlParseError } from './edl/parser';
import { EdlLexError } from './edl/lexer';
import { serializeScenario } from './edl/serializer';
import { NonAsciiError } from './edl/ascii-guard';
import { preprocessIncludes, findIncludeNames } from './edl/preprocessor';
import { useScenarioStore } from './store/scenarioStore';
import { inferTopology } from './model/selectors';
import { findNodeById } from './model/treeOps';
import { ScenarioTree } from './components/tree/ScenarioTree';
import { PaletteTabs } from './components/palette/PaletteTabs';
import { SlotForm } from './components/inspector/SlotForm';
import { ScenarioMap } from './components/map/ScenarioMap';
import { TopologySelector } from './components/topology/TopologySelector';
import type { ScenarioFileInfo } from '../../shared/apiTypes';

function downloadText(filename: string, text: string) {
  const blob = new Blob([text], { type: 'text/plain;charset=us-ascii' });
  const url = URL.createObjectURL(blob);
  const a = window.document.createElement('a');
  a.href = url;
  a.download = filename;
  a.click();
  URL.revokeObjectURL(url);
}

export function App() {
  const [scenarios, setScenarios] = useState<ScenarioFileInfo[]>([]);
  const [loadError, setLoadError] = useState<string | null>(null);
  const [exportError, setExportError] = useState<string | null>(null);

  const document = useScenarioStore((s) => s.document);
  const sourcePath = useScenarioStore((s) => s.sourcePath);
  const dirty = useScenarioStore((s) => s.dirty);
  const selectedNodeId = useScenarioStore((s) => s.selectedNodeId);
  const loadDocument = useScenarioStore((s) => s.loadDocument);
  const undo = useScenarioStore((s) => s.undo);

  useEffect(() => {
    api.listScenarios().then(setScenarios).catch((e) => setLoadError(String(e)));
  }, []);

  async function handleImport(path: string) {
    setLoadError(null);
    try {
      const { text } = await api.getScenarioContent(path);
      const includeNames = findIncludeNames(text);
      const fragments: Record<string, string> = {};
      for (const name of includeNames) {
        const frag = await api.getFragment(name);
        fragments[name] = frag.text;
      }
      const { text: resolved } = preprocessIncludes(text, fragments);
      const node = parseScenario(resolved);
      loadDocument(node, path, inferTopology(node));
    } catch (e) {
      const msg =
        e instanceof EdlParseError || e instanceof EdlLexError
          ? `erro de sintaxe: ${e.message}`
          : e instanceof Error
            ? e.message
            : String(e);
      setLoadError(msg);
    }
  }

  function handleExport() {
    if (!document) return;
    setExportError(null);
    try {
      const text = serializeScenario(document);
      const filename = sourcePath ? sourcePath.split('/').pop()! : 'scenario.epp';
      downloadText(filename, text);
    } catch (e) {
      setExportError(e instanceof NonAsciiError ? e.message : e instanceof Error ? e.message : String(e));
    }
  }

  const selected = document && selectedNodeId ? findNodeById(document, selectedNodeId) : undefined;

  return (
    <div className="app">
      <header className="app__topbar">
        <h1>poc-mixr -- editor de cenarios</h1>
        <select
          value={sourcePath ?? ''}
          onChange={(e) => {
            if (e.target.value) void handleImport(e.target.value);
          }}
        >
          <option value="" disabled>
            importar cenario existente...
          </option>
          {scenarios.map((s) => (
            <option key={s.path} value={s.path}>
              [{s.group}] {s.path}
            </option>
          ))}
        </select>
        <TopologySelector />
        <button type="button" onClick={undo} disabled={!document}>
          desfazer
        </button>
        <button type="button" onClick={handleExport} disabled={!document}>
          exportar .epp{dirty ? ' *' : ''}
        </button>
        {loadError && <span className="app__error">{loadError}</span>}
        {exportError && <span className="app__error">{exportError}</span>}
      </header>

      <div className="app__body">
        <aside className="app__sidebar-left">
          <ScenarioTree />
          <PaletteTabs />
        </aside>

        <main className="app__center">
          <ScenarioMap />
        </main>

        <aside className="app__sidebar-right">
          {selected ? <SlotForm node={selected} /> : <p className="app__no-selection">selecione um node na arvore ou no mapa</p>}
        </aside>
      </div>
    </div>
  );
}
