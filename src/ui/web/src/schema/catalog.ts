import type { FactorySchema } from './types';
import { stationSchemas } from './station.schema';
import { aircraftSchemas } from './aircraft.schema';
import { weaponSchemas } from './weapon.schema';
import { ubfSchemas } from './ubf.schema';
import { disSchemas } from './dis.schema';
import { msgSchemas } from './msg.schema';
import { tacviewSchemas } from './tacview.schema';
import { joystickSchemas } from './joystick.schema';
import { pluginSchemas } from './plugin.schema';

export * from './types';
export { KNOWN_PLUGIN_PRESETS } from './plugin.schema';

const ALL_SCHEMAS: FactorySchema[] = [
  ...stationSchemas,
  ...aircraftSchemas,
  ...weaponSchemas,
  ...ubfSchemas,
  ...disSchemas,
  ...msgSchemas,
  ...tacviewSchemas,
  ...joystickSchemas,
  ...pluginSchemas,
];

/** Indice por nome de fabrica -- a fonte da verdade que parser/serializer/palette/inspector consultam. */
export const CATALOG: Record<string, FactorySchema> = Object.fromEntries(
  ALL_SCHEMAS.map((s) => [s.factoryName, s]),
);

export function getFactorySchema(factoryName: string): FactorySchema | undefined {
  return CATALOG[factoryName];
}

export function listFactoriesByGroup(group: FactorySchema['group']): FactorySchema[] {
  return ALL_SCHEMAS.filter((s) => s.group === group);
}

/**
 * true se `factoryName` pode ser inserido no slot `parentSlot` de um node cujo
 * factoryName e `parentFactoryName`. Usado pela paleta (canInsertHere) para
 * habilitar/desabilitar cards conforme a selecao atual na arvore.
 */
export function canInsertInto(parentFactoryName: string, parentSlot: string, factoryName: string): boolean {
  const schema = getFactorySchema(factoryName);
  if (!schema) return false;
  return schema.insertionPoints.some(
    (rule) => rule.parentFactory === parentFactoryName && rule.parentSlot === parentSlot,
  );
}

/** Todos os factoryName que podem ser inseridos em algum slot do node pai dado. */
export function insertableFactoriesFor(parentFactoryName: string): Array<{ factoryName: string; slot: string }> {
  const out: Array<{ factoryName: string; slot: string }> = [];
  for (const schema of ALL_SCHEMAS) {
    for (const rule of schema.insertionPoints) {
      if (rule.parentFactory === parentFactoryName) {
        out.push({ factoryName: schema.factoryName, slot: rule.parentSlot });
      }
    }
  }
  return out;
}
