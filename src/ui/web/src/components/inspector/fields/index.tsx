import type { ChangeEvent } from 'react';

export function NumberField({ value, onChange }: { value: number; onChange: (v: number) => void }) {
  return (
    <input
      type="number"
      value={Number.isFinite(value) ? value : ''}
      onChange={(e: ChangeEvent<HTMLInputElement>) => onChange(Number(e.target.value))}
    />
  );
}

export function TextField({ value, onChange }: { value: string; onChange: (v: string) => void }) {
  return <input type="text" value={value} onChange={(e) => onChange(e.target.value)} />;
}

export function BoolField({ value, onChange }: { value: boolean; onChange: (v: boolean) => void }) {
  return <input type="checkbox" checked={value} onChange={(e) => onChange(e.target.checked)} />;
}

export function UnitNumberField({
  value,
  unit,
  allowedUnits,
  onChange,
}: {
  value: number;
  unit: string;
  allowedUnits?: string[];
  onChange: (v: number, unit: string) => void;
}) {
  const options = allowedUnits && allowedUnits.includes(unit) ? allowedUnits : [unit, ...(allowedUnits ?? [])];
  return (
    <span className="field-unit-number">
      <input type="number" value={Number.isFinite(value) ? value : ''} onChange={(e) => onChange(Number(e.target.value), unit)} />
      <select value={unit} onChange={(e) => onChange(value, e.target.value)}>
        {options.map((u) => (
          <option key={u} value={u}>
            {u}
          </option>
        ))}
      </select>
    </span>
  );
}

export function VectorField({ values, onChange }: { values: number[]; onChange: (v: number[]) => void }) {
  const text = values.join(' ');
  return (
    <input
      type="text"
      value={text}
      onChange={(e) => {
        const parsed = e.target.value
          .split(/\s+/)
          .filter((s) => s.length > 0)
          .map(Number)
          .filter((n) => !Number.isNaN(n));
        onChange(parsed);
      }}
    />
  );
}

export function PlayerRefField({
  value,
  players,
  onChange,
}: {
  value: string;
  players: string[];
  onChange: (v: string) => void;
}) {
  return (
    <select value={value} onChange={(e) => onChange(e.target.value)}>
      {!players.includes(value) && value && <option value={value}>{value} (nao encontrado)</option>}
      {players.map((p) => (
        <option key={p} value={p}>
          {p}
        </option>
      ))}
    </select>
  );
}
