import type { FactorySchema } from './types';

export const AircraftSchema: FactorySchema = {
  factoryName: 'Aircraft',
  baseClass: 'mixr::models::AirVehicle',
  group: 'player',
  label: 'Aviao (Aircraft)',
  slots: [
    { name: 'id', valueShape: 'number', required: true },
    { name: 'side', valueShape: 'identifier', required: true, default: 'blue' },
    { name: 'type', valueShape: 'string', required: true, default: 'C310' },
    { name: 'signature', valueShape: 'form', allowedFactories: ['SigSphere'] },
    {
      name: 'initXPos',
      valueShape: 'unitNumber',
      unitFamily: 'Distance',
      allowedUnits: ['NauticalMiles', 'Meters', 'Feet'],
      note: 'offset relativo ao latitude/longitude da WorldModel -- alternativa a initLatitude/initLongitude',
    },
    { name: 'initYPos', valueShape: 'unitNumber', unitFamily: 'Distance', allowedUnits: ['NauticalMiles', 'Meters', 'Feet'] },
    { name: 'initAlt', valueShape: 'unitNumber', unitFamily: 'Distance', allowedUnits: ['Meters', 'Feet'], required: true },
    { name: 'initLatitude', valueShape: 'number', note: 'alternativa geodesica direta a initXPos/initYPos -- ver map/coords.ts' },
    { name: 'initLongitude', valueShape: 'number' },
    { name: 'initHeading', valueShape: 'unitNumber', unitFamily: 'Angle', allowedUnits: ['Degrees'], required: true },
    { name: 'initVelocity', valueShape: 'number', note: 'nos (knots) -- sem unidade no EDL real do repo' },
    { name: 'dataLogTime', valueShape: 'unitNumber', unitFamily: 'Time', allowedUnits: ['Seconds'], default: 0.1,
      note: 'sem isso o player NUNCA emite REID_PLAYER_DATA e some do Tacview -- armadilha documentada' },
    { name: 'interpolateTerrain', valueShape: 'boolean', default: true },
    {
      name: 'components',
      valueShape: 'namedList',
      allowedFactories: [
        'JSBSimModel', 'Autopilot', 'AlertDatalink', 'Gimbal', 'SensorMgr',
        'OnboardComputer', 'StoresMgr', 'FlightAgentTC',
      ],
      note: 'ordem observada nos cenarios reais: dynamicsModel, pilot, datalink, antennas, sensors, obc, stores, agent (agent sempre por ultimo)',
    },
  ],
  insertionPoints: [{ parentFactory: 'WorldModel', parentSlot: 'players', shape: 'namedList' }],
};

export const JSBSimModelSchema: FactorySchema = {
  factoryName: 'JSBSimModel',
  baseClass: 'mixr::models::JSBSimModel',
  group: 'subsystem',
  label: 'Dinamica de voo (JSBSim)',
  slots: [
    { name: 'rootDir', valueShape: 'string', required: true, default: './dist/share/mixr-plugins/flight/jsbsim/' },
    { name: 'model', valueShape: 'string', required: true, note: 'nome da pasta em models/plugins/data/flight/jsbsim/aircraft/ -- ver /api/aircraft' },
    { name: 'debugLevel', valueShape: 'number', default: 0 },
  ],
  insertionPoints: [{ parentFactory: 'Aircraft', parentSlot: 'components', shape: 'namedList' }],
};

export const AutopilotSchema: FactorySchema = {
  factoryName: 'Autopilot',
  baseClass: 'mixr::models::Autopilot',
  group: 'subsystem',
  label: 'Piloto automatico (Autopilot)',
  slots: [
    { name: 'navMode', valueShape: 'boolean', default: false },
    { name: 'headingHoldMode', valueShape: 'boolean', default: true },
    { name: 'altitudeHoldMode', valueShape: 'boolean', default: true },
    { name: 'velocityHoldMode', valueShape: 'boolean', default: true },
    { name: 'holdHeading', valueShape: 'unitNumber', unitFamily: 'Angle', allowedUnits: ['Degrees'] },
    { name: 'holdAltitude', valueShape: 'unitNumber', unitFamily: 'Distance', allowedUnits: ['Meters', 'Feet'] },
    { name: 'holdVelocityKts', valueShape: 'number' },
    { name: 'maxRateOfTurnDps', valueShape: 'number', default: 3.0 },
    { name: 'maxBankAngle', valueShape: 'number', default: 30.0 },
    { name: 'maxPitchAngle', valueShape: 'number', default: 10.0 },
    { name: 'maxClimbRateMps', valueShape: 'number', default: 8.0 },
    { name: 'maxAcceleration', valueShape: 'number', default: 2.0 },
  ],
  insertionPoints: [{ parentFactory: 'Aircraft', parentSlot: 'components', shape: 'namedList' }],
};

export const SigSphereSchema: FactorySchema = {
  factoryName: 'SigSphere',
  baseClass: 'mixr::models::SigSphere',
  group: 'subsystem',
  label: 'Assinatura de radar (SigSphere)',
  slots: [{ name: 'radius', valueShape: 'number', required: true, default: 3.0 }],
  insertionPoints: [
    { parentFactory: 'Aircraft', parentSlot: 'signature', shape: 'form' },
    { parentFactory: 'GuidedMissile', parentSlot: 'signature', shape: 'form' },
  ],
};

export const GimbalSchema: FactorySchema = {
  factoryName: 'Gimbal',
  baseClass: 'mixr::models::Gimbal',
  group: 'subsystem',
  label: 'Antena giratoria (Gimbal)',
  slots: [{ name: 'components', valueShape: 'namedList', allowedFactories: ['Antenna'] }],
  insertionPoints: [{ parentFactory: 'Aircraft', parentSlot: 'components', shape: 'namedList' }],
};

export const AntennaSchema: FactorySchema = {
  factoryName: 'Antenna',
  baseClass: 'mixr::models::Antenna',
  group: 'subsystem',
  label: 'Antena de radar',
  slots: [
    { name: 'polarization', valueShape: 'identifier', default: 'horizontal' },
    { name: 'gain', valueShape: 'unitNumber', unitFamily: 'Ratio', allowedUnits: ['dB'], default: 42 },
    { name: 'gainPatternDeg', valueShape: 'boolean', default: false },
    { name: 'initPosition', valueShape: 'rawVector', note: 'vetor [az el], tipicamente [ 0 0 ]' },
    { name: 'maxRates', valueShape: 'rawVector' },
    { name: 'commandRateAzimuth', valueShape: 'unitNumber', unitFamily: 'Angle', allowedUnits: ['Degrees'], default: 60 },
    { name: 'commandRateElevation', valueShape: 'unitNumber', unitFamily: 'Angle', allowedUnits: ['Degrees'], default: 60 },
    { name: 'reference', valueShape: 'rawVector' },
    { name: 'searchVolume', valueShape: 'rawVector' },
    { name: 'numBars', valueShape: 'number', default: 2 },
    { name: 'maxPlayersOfInterest', valueShape: 'number', default: 8 },
    { name: 'playerOfInterestTypes', valueShape: 'positionalList', note: 'lista de Identifier, ex: { air }' },
    { name: 'maxRange2PlayersOfInterest', valueShape: 'unitNumber', unitFamily: 'Distance', allowedUnits: ['NauticalMiles'], default: 40 },
    { name: 'maxAngle2PlayersOfInterest', valueShape: 'unitNumber', unitFamily: 'Angle', allowedUnits: ['Degrees'], default: 60 },
    { name: 'localPlayersOfInterestOnly', valueShape: 'boolean', default: false },
    { name: 'useWorldCoordinates', valueShape: 'boolean', default: false },
  ],
  insertionPoints: [{ parentFactory: 'Gimbal', parentSlot: 'components', shape: 'namedList' }],
};

export const SensorMgrSchema: FactorySchema = {
  factoryName: 'SensorMgr',
  baseClass: 'mixr::models::SensorMgr',
  group: 'subsystem',
  label: 'Gerente de sensores (SensorMgr)',
  slots: [{ name: 'components', valueShape: 'positionalList', allowedFactories: ['Tws'] }],
  insertionPoints: [{ parentFactory: 'Aircraft', parentSlot: 'components', shape: 'namedList' }],
};

export const TwsSchema: FactorySchema = {
  factoryName: 'Tws',
  baseClass: 'mixr::models::Tws',
  group: 'subsystem',
  label: 'Radar de busca (Tws)',
  slots: [
    { name: 'trackManagerName', valueShape: 'identifier', required: true },
    { name: 'antennaName', valueShape: 'identifier', required: true },
    { name: 'powerPeak', valueShape: 'unitNumber', unitFamily: 'Power', allowedUnits: ['KiloWatts'] },
    { name: 'frequency', valueShape: 'unitNumber', unitFamily: 'Frequency', allowedUnits: ['GigaHertz'] },
    { name: 'bandwidth', valueShape: 'unitNumber', unitFamily: 'Frequency', allowedUnits: ['GigaHertz'] },
    { name: 'PRF', valueShape: 'unitNumber', unitFamily: 'Frequency', allowedUnits: ['Hertz'] },
    { name: 'pulseWidth', valueShape: 'unitNumber', unitFamily: 'Time', allowedUnits: ['MilliSeconds'] },
    { name: 'threshold', valueShape: 'unitNumber', unitFamily: 'Ratio', allowedUnits: ['dB'] },
    { name: 'igain', valueShape: 'unitNumber', unitFamily: 'Ratio', allowedUnits: ['dB'] },
    { name: 'ranges', valueShape: 'rawVector' },
    { name: 'initRangeIdx', valueShape: 'number' },
  ],
  insertionPoints: [{ parentFactory: 'SensorMgr', parentSlot: 'components', shape: 'positionalList' }],
};

export const OnboardComputerSchema: FactorySchema = {
  factoryName: 'OnboardComputer',
  baseClass: 'mixr::models::OnboardComputer',
  group: 'subsystem',
  label: 'Computador de bordo',
  slots: [{ name: 'components', valueShape: 'namedList', allowedFactories: ['AirTrkMgr'] }],
  insertionPoints: [{ parentFactory: 'Aircraft', parentSlot: 'components', shape: 'namedList' }],
};

export const AirTrkMgrSchema: FactorySchema = {
  factoryName: 'AirTrkMgr',
  baseClass: 'mixr::models::AirTrkMgr',
  group: 'subsystem',
  label: 'Gerente de pistas (AirTrkMgr)',
  slots: [
    { name: 'maxTracks', valueShape: 'number', default: 20 },
    { name: 'alpha', valueShape: 'number', default: 1.0 },
    { name: 'beta', valueShape: 'number', default: 0.5 },
    { name: 'gamma', valueShape: 'number', default: 0.0 },
    { name: 'positionGate', valueShape: 'number' },
    { name: 'rangeGate', valueShape: 'number' },
    { name: 'velocityGate', valueShape: 'number' },
    { name: 'firstTrackId', valueShape: 'number', default: 1000 },
  ],
  insertionPoints: [{ parentFactory: 'OnboardComputer', parentSlot: 'components', shape: 'namedList' }],
};

// nome de fabrica EDL e "StoresMgr", NAO "SimpleStoresMgr" -- armadilha
// documentada no CLAUDE.md (IMPLEMENT_SUBCLASS(SimpleStoresMgr,"StoresMgr")).
export const StoresMgrSchema: FactorySchema = {
  factoryName: 'StoresMgr',
  baseClass: 'mixr::models::SimpleStoresMgr',
  group: 'subsystem',
  label: 'Gerente de armamentos (StoresMgr)',
  slots: [
    { name: 'numStations', valueShape: 'number', required: true, default: 1 },
    { name: 'stores', valueShape: 'namedList', allowedFactories: ['GuidedMissile'], note: 'chave e o numero da estacao: 1, 2, ...' },
  ],
  insertionPoints: [{ parentFactory: 'Aircraft', parentSlot: 'components', shape: 'namedList' }],
};

export const aircraftSchemas: FactorySchema[] = [
  AircraftSchema, JSBSimModelSchema, AutopilotSchema, SigSphereSchema,
  GimbalSchema, AntennaSchema, SensorMgrSchema, TwsSchema,
  OnboardComputerSchema, AirTrkMgrSchema, StoresMgrSchema,
];
