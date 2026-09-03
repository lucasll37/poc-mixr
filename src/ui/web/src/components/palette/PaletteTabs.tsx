import { useState } from 'react';
import type { FactoryGroup } from '../../schema/catalog';
import { listFactoriesByGroup } from '../../schema/catalog';
import { PaletteCard } from './PaletteCard';

const GROUPS: Array<{ key: FactoryGroup; label: string }> = [
  { key: 'player', label: 'Aviao' },
  { key: 'subsystem', label: 'Subsistemas' },
  { key: 'weapon', label: 'Missil' },
  { key: 'ubf', label: 'Decisao' },
  { key: 'dis', label: 'DIS' },
  { key: 'msg', label: 'Mensagens' },
  { key: 'tacview', label: 'Tacview' },
  { key: 'joystick', label: 'Joystick' },
  { key: 'plugin', label: 'Plugin' },
  { key: 'station', label: 'Mundo/Terreno' },
];

export function PaletteTabs() {
  const [active, setActive] = useState<FactoryGroup>('player');
  const schemas = listFactoriesByGroup(active);

  return (
    <div className="palette-tabs">
      <div className="palette-tabs__bar">
        {GROUPS.map((g) => (
          <button
            key={g.key}
            type="button"
            className={`palette-tabs__tab${active === g.key ? ' palette-tabs__tab--active' : ''}`}
            onClick={() => setActive(g.key)}
          >
            {g.label}
          </button>
        ))}
      </div>
      <div className="palette-tabs__cards">
        {schemas.map((s) => (
          <PaletteCard key={s.factoryName} schema={s} />
        ))}
        {schemas.length === 0 && <p className="palette-tabs__empty">nenhum elemento neste grupo</p>}
      </div>
    </div>
  );
}
