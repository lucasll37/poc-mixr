import { describe, expect, it } from 'vitest';
import { latLonToLocalOffset, localOffsetToLatLon, nmToMeters } from '../coords';

const REF_LAT = -22.25;
const REF_LON = -42.48;

describe('conversao lat/lon <-> offset NM (ponto de referencia real do repo)', () => {
  it('ida-e-volta com erro menor que 1 metro para os offsets reais usados nos cenarios', () => {
    for (const nm of [0, 5.0, -5.0, 12.0, -12.0]) {
      const northM = nmToMeters(nm);
      const eastM = nmToMeters(nm);
      const { lat, lon } = localOffsetToLatLon(REF_LAT, REF_LON, northM, eastM);
      const back = latLonToLocalOffset(REF_LAT, REF_LON, lat, lon);
      expect(Math.abs(back.northM - northM)).toBeLessThan(1);
      expect(Math.abs(back.eastM - eastM)).toBeLessThan(1);
    }
  });

  it('offset zero cai exatamente no ponto de referencia', () => {
    const { lat, lon } = localOffsetToLatLon(REF_LAT, REF_LON, 0, 0);
    expect(lat).toBeCloseTo(REF_LAT, 9);
    expect(lon).toBeCloseTo(REF_LON, 9);
  });
});
