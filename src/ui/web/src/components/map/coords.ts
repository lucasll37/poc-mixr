// Conversao lat/lon <-> offset local NED (North-East-Down, convencao do
// MIXR: initXPos = norte, initYPos = leste -- confirmado pelos cenarios
// reais, ex: falcon1 com initXPos:5NM initYPos:0NM initHeading:90 (=leste),
// se afastando do ponto de referencia exatamente pro leste conforme o
// heading indica).
//
// Projecao local plana equirretangular -- suficiente na escala de dezenas de
// NM usada nos cenarios reais (erro da ordem de poucos metros, irrelevante
// frente a resolucao dos tiles SRTM, 30-90m). NAO usar para distancias
// grandes (> ~100 NM) ou perto dos polos.
const METERS_PER_DEGREE_LAT = 111_320; // aproximacao padrao, WGS84 medio
const NM_TO_METERS = 1852;

export interface LatLon {
  lat: number;
  lon: number;
}

export function nmToMeters(nm: number): number {
  return nm * NM_TO_METERS;
}

export function metersToNm(m: number): number {
  return m / NM_TO_METERS;
}

/** (northM, eastM) relativos a (refLat, refLon) -> lat/lon absolutos. */
export function localOffsetToLatLon(refLat: number, refLon: number, northM: number, eastM: number): LatLon {
  const lat = refLat + northM / METERS_PER_DEGREE_LAT;
  const metersPerDegreeLon = METERS_PER_DEGREE_LAT * Math.cos((refLat * Math.PI) / 180);
  const lon = refLon + eastM / metersPerDegreeLon;
  return { lat, lon };
}

/** lat/lon absolutos -> (northM, eastM) relativos a (refLat, refLon). */
export function latLonToLocalOffset(
  refLat: number,
  refLon: number,
  lat: number,
  lon: number,
): { northM: number; eastM: number } {
  const northM = (lat - refLat) * METERS_PER_DEGREE_LAT;
  const metersPerDegreeLon = METERS_PER_DEGREE_LAT * Math.cos((refLat * Math.PI) / 180);
  const eastM = (lon - refLon) * metersPerDegreeLon;
  return { northM, eastM };
}
