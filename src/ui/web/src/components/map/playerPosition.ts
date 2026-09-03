import type { ScenarioNode } from '../../model/ScenarioNode';
import { localOffsetToLatLon, latLonToLocalOffset } from './coords';

const METERS_PER_UNIT: Record<string, number> = {
  Meters: 1,
  NauticalMiles: 1852,
  Feet: 0.3048,
  Kilometers: 1000,
};

function toMeters(unit: string, value: number): number {
  return value * (METERS_PER_UNIT[unit] ?? 1);
}

function fromMeters(unit: string, meters: number): number {
  return meters / (METERS_PER_UNIT[unit] ?? 1);
}

export type PositionMode = 'xy' | 'latlon' | 'none';

export function getPlayerPositionMode(player: ScenarioNode): PositionMode {
  if (player.slots.initLatitude && player.slots.initLongitude) return 'latlon';
  if (player.slots.initXPos && player.slots.initYPos) return 'xy';
  return 'none';
}

/** Posicao absoluta (lat/lon) de um player -- deriva de initXPos/initYPos (+ referencia da WorldModel)
 *  ou de initLatitude/initLongitude direto, conforme a forma de slot que o node ja usa. */
export function getPlayerLatLon(
  player: ScenarioNode,
  refLat: number,
  refLon: number,
): { lat: number; lon: number } | undefined {
  const mode = getPlayerPositionMode(player);
  if (mode === 'latlon') {
    const lat = player.slots.initLatitude;
    const lon = player.slots.initLongitude;
    if (lat?.kind === 'number' && lon?.kind === 'number') return { lat: lat.value, lon: lon.value };
    return undefined;
  }
  if (mode === 'xy') {
    const x = player.slots.initXPos;
    const y = player.slots.initYPos;
    if (x?.kind === 'unitNumber' && y?.kind === 'unitNumber') {
      const northM = toMeters(x.unit, x.value);
      const eastM = toMeters(y.unit, y.value);
      return localOffsetToLatLon(refLat, refLon, northM, eastM);
    }
    if (x?.kind === 'number' && y?.kind === 'number') {
      // sem unidade explicita -- assume metros (raro nos .epp reais, mas nao deve travar a UI)
      return localOffsetToLatLon(refLat, refLon, x.value, y.value);
    }
    return undefined;
  }
  return undefined;
}

/**
 * Devolve os slots atualizados (initXPos/initYPos OU initLatitude/initLongitude,
 * NUNCA os dois) preservando a forma/unidade ORIGINAL do player -- arrastar o
 * marcador nunca migra silenciosamente de uma representacao pra outra.
 */
export function withPlayerLatLon(
  player: ScenarioNode,
  lat: number,
  lon: number,
  refLat: number,
  refLon: number,
): { slotKey: string; value: ScenarioNode['slots'][string] }[] {
  const mode = getPlayerPositionMode(player);
  if (mode === 'latlon') {
    return [
      { slotKey: 'initLatitude', value: { kind: 'number', value: lat } },
      { slotKey: 'initLongitude', value: { kind: 'number', value: lon } },
    ];
  }
  // default (mode 'xy' ou 'none'): grava em initXPos/initYPos, preservando a
  // unidade original quando ja existia.
  const xUnit = player.slots.initXPos?.kind === 'unitNumber' ? player.slots.initXPos.unit : 'NauticalMiles';
  const yUnit = player.slots.initYPos?.kind === 'unitNumber' ? player.slots.initYPos.unit : 'NauticalMiles';
  const { northM, eastM } = latLonToLocalOffset(refLat, refLon, lat, lon);
  return [
    { slotKey: 'initXPos', value: { kind: 'unitNumber', unit: xUnit, value: fromMeters(xUnit, northM) } },
    { slotKey: 'initYPos', value: { kind: 'unitNumber', unit: yUnit, value: fromMeters(yUnit, eastM) } },
  ];
}
