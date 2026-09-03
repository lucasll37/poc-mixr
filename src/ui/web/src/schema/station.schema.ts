import type { FactorySchema } from './types';

// ClockStation: EMPTY_SLOTTABLE, herda tudo de mixr::simulation::Station --
// ver shared/xclock/ClockStation.cpp:11 e CLAUDE.md secao "shared/xclock".
export const ClockStationSchema: FactorySchema = {
  factoryName: 'ClockStation',
  baseClass: 'mixr::simulation::Station',
  group: 'station',
  label: 'Station (com controle de tempo)',
  slots: [
    { name: 'tcPriority', valueShape: 'number', default: 0.5 },
    { name: 'ownship', valueShape: 'playerRef', note: 'nome do player principal (usado por ex. no Tacview)' },
    { name: 'fastForwardRate', valueShape: 'number', note: 'multiplicador nativo de aceleracao de tempo' },
    { name: 'simulation', valueShape: 'form', required: true, allowedFactories: ['WorldModel'] },
    {
      name: 'components',
      valueShape: 'namedList',
      allowedFactories: ['PluginLoader', 'SimAgent', 'MsgFeed'],
      note: 'plugins: deve vir ANTES de qualquer uso das classes que ele fornece -- ver edl/ordering.ts',
    },
    { name: 'ioHandler', valueShape: 'form', allowedFactories: ['JoystickIoHandler'] },
    { name: 'networks', valueShape: 'positionalList', allowedFactories: ['DisNetIO'] },
    { name: 'dataRecorder', valueShape: 'form', allowedFactories: ['ExposedDataRecorder'] },
  ],
  insertionPoints: [],
};

// Station (nativo, sem controle de tempo) -- usado por src/poc/bandit-dis, que
// tem piloto humano/joystick e nao precisa da UI de pausa/aceleracao.
export const StationSchema: FactorySchema = {
  ...ClockStationSchema,
  factoryName: 'Station',
  label: 'Station (sem controle de tempo)',
};

export const WorldModelSchema: FactorySchema = {
  factoryName: 'WorldModel',
  baseClass: 'mixr::models::WorldModel',
  group: 'station',
  label: 'Mundo (WorldModel)',
  slots: [
    { name: 'latitude', valueShape: 'number', required: true, note: 'ponto de referencia absoluto do cenario' },
    { name: 'longitude', valueShape: 'number', required: true },
    { name: 'numTcThreads', valueShape: 'number', default: 4 },
    { name: 'terrain', valueShape: 'form', allowedFactories: ['SrtmHgtFile'] },
    { name: 'players', valueShape: 'namedList', allowedFactories: ['Aircraft'] },
  ],
  insertionPoints: [{ parentFactory: 'ClockStation', parentSlot: 'simulation', shape: 'form' }],
};

// slots reais sao 'file'/'path', NAO 'filename'/'pathname' -- armadilha
// documentada no CLAUDE.md (Terrain.cpp:24-32).
export const SrtmHgtFileSchema: FactorySchema = {
  factoryName: 'SrtmHgtFile',
  baseClass: 'mixr::terrain::Terrain',
  group: 'station',
  label: 'Terreno (tile SRTM)',
  requiresNativeFactories: ['mixr::terrain::factory'],
  slots: [
    { name: 'path', valueShape: 'string', required: true, default: './shared/data/terrain/srtm/' },
    { name: 'file', valueShape: 'string', required: true, note: 'ex: "S23W043.hgt" -- tamanho em bytes e validado' },
  ],
  insertionPoints: [{ parentFactory: 'WorldModel', parentSlot: 'terrain', shape: 'form' }],
};

export const stationSchemas: FactorySchema[] = [
  ClockStationSchema,
  StationSchema,
  WorldModelSchema,
  SrtmHgtFileSchema,
];
