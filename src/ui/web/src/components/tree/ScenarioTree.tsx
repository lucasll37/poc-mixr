import type { ScenarioNode } from '../../model/ScenarioNode';
import { listChildren } from '../../model/treeOps';
import { useScenarioStore } from '../../store/scenarioStore';

function TreeNode({ node, label, depth }: { node: ScenarioNode; label: string; depth: number }) {
  const selectedNodeId = useScenarioStore((s) => s.selectedNodeId);
  const select = useScenarioStore((s) => s.select);
  const children = listChildren(node);
  const isSelected = node.id === selectedNodeId;

  return (
    <div className="tree-node">
      <button
        type="button"
        className={`tree-node__row${isSelected ? ' tree-node__row--selected' : ''}`}
        style={{ paddingLeft: `${depth * 14 + 6}px` }}
        onClick={() => select(node.id)}
      >
        <span className="tree-node__label">{label}</span>
        <span className="tree-node__factory">{node.factoryName}</span>
        {node.unknown && <span className="tree-node__unknown-badge">?</span>}
      </button>
      {children.map((c, i) => (
        <TreeNode key={`${c.slotKey}-${i}-${c.node.id}`} node={c.node} label={c.label} depth={depth + 1} />
      ))}
    </div>
  );
}

export function ScenarioTree() {
  const document = useScenarioStore((s) => s.document);
  if (!document) return <div className="tree-empty">nenhum cenario carregado</div>;
  return (
    <div className="scenario-tree">
      <TreeNode node={document} label={document.formName ?? 'cenario'} depth={0} />
    </div>
  );
}
