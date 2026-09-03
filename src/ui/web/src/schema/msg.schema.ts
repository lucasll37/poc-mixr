import type { FactorySchema } from './types';

export const MsgFileSinkSchema: FactorySchema = {
  factoryName: 'MsgFileSink',
  baseClass: 'mixr::xmsg::MsgFileSink',
  group: 'msg',
  label: 'Destino em arquivo (MsgFileSink)',
  slots: [
    { name: 'fileName', valueShape: 'string', required: true },
    { name: 'flushEvery', valueShape: 'unitNumber', unitFamily: 'Time', allowedUnits: ['Seconds'], default: 2 },
  ],
  insertionPoints: [{ parentFactory: 'MsgFeed', parentSlot: 'sinks', shape: 'positionalList' }],
};

export const MsgChangedSchema: FactorySchema = {
  factoryName: 'MsgChanged',
  baseClass: 'mixr::xmsg::Condition',
  group: 'msg',
  label: 'Condicao: mudou (MsgChanged)',
  slots: [
    { name: 'field', valueShape: 'identifier', required: true },
    { name: 'by', valueShape: 'number', required: true, note: 'deadband desde o ultimo valor emitido' },
  ],
  insertionPoints: [{ parentFactory: 'MsgReport', parentSlot: 'when', shape: 'positionalList' }],
};

export const MsgThresholdSchema: FactorySchema = {
  factoryName: 'MsgThreshold',
  baseClass: 'mixr::xmsg::Condition',
  group: 'msg',
  label: 'Condicao: limiar (MsgThreshold)',
  slots: [
    { name: 'field', valueShape: 'identifier', required: true },
    { name: 'above', valueShape: 'number' },
    { name: 'below', valueShape: 'number' },
    { name: 'clear', valueShape: 'number', required: true, note: 'histerese -- ate onde precisa voltar para rearmar' },
    { name: 'hold', valueShape: 'unitNumber', unitFamily: 'Time', allowedUnits: ['Seconds'] },
  ],
  insertionPoints: [{ parentFactory: 'MsgReport', parentSlot: 'when', shape: 'positionalList' }],
};

export const MsgRateSchema: FactorySchema = {
  factoryName: 'MsgRate',
  baseClass: 'mixr::xmsg::Condition',
  group: 'msg',
  label: 'Condicao: taxa (MsgRate)',
  slots: [
    { name: 'field', valueShape: 'identifier', required: true },
    { name: 'above', valueShape: 'number' },
    { name: 'below', valueShape: 'number' },
    { name: 'clear', valueShape: 'number', required: true },
    { name: 'window', valueShape: 'unitNumber', unitFamily: 'Time', allowedUnits: ['Seconds'], required: true },
    { name: 'hold', valueShape: 'unitNumber', unitFamily: 'Time', allowedUnits: ['Seconds'] },
  ],
  insertionPoints: [{ parentFactory: 'MsgReport', parentSlot: 'when', shape: 'positionalList' }],
};

export const MsgReportSchema: FactorySchema = {
  factoryName: 'MsgReport',
  baseClass: 'mixr::xmsg::MsgReport',
  group: 'msg',
  label: 'Mensagem (MsgReport)',
  slots: [
    { name: 'name', valueShape: 'identifier', required: true },
    { name: 'players', valueShape: 'positionalList', note: 'lista de nomes de player -- vazio = todos' },
    { name: 'labels', valueShape: 'positionalList', note: 'ex: { player side mode }' },
    { name: 'fields', valueShape: 'positionalList', required: true,
      note: 'nomes de campo de telemetria: latDeg, lonDeg, altMslM, altAglM, hdgDeg, speedKts, machNum, fuelFrac, ...' },
    { name: 'when', valueShape: 'positionalList', allowedFactories: ['MsgChanged', 'MsgThreshold', 'MsgRate'] },
    { name: 'match', valueShape: 'identifier', default: 'all' },
    { name: 'every', valueShape: 'unitNumber', unitFamily: 'Time', allowedUnits: ['Seconds'], note: 'cadencia MAXIMA -- nao descarta borda, so adia' },
  ],
  insertionPoints: [{ parentFactory: 'MsgFeed', parentSlot: 'messages', shape: 'positionalList' }],
};

export const MsgFeedSchema: FactorySchema = {
  factoryName: 'MsgFeed',
  baseClass: 'mixr::xmsg::MsgFeed',
  group: 'msg',
  label: 'Feed de mensagens (MsgFeed)',
  slots: [
    { name: 'trackManager', valueShape: 'identifier' },
    { name: 'maxPlayers', valueShape: 'number', default: 64 },
    { name: 'healthEvery', valueShape: 'unitNumber', unitFamily: 'Time', allowedUnits: ['Seconds'], default: 10 },
    { name: 'sinks', valueShape: 'positionalList', allowedFactories: ['MsgFileSink'], required: true },
    { name: 'messages', valueShape: 'positionalList', allowedFactories: ['MsgReport'] },
  ],
  insertionPoints: [{ parentFactory: 'ClockStation', parentSlot: 'components', shape: 'namedList' }],
};

export const msgSchemas: FactorySchema[] = [
  MsgFileSinkSchema, MsgChangedSchema, MsgThresholdSchema, MsgRateSchema, MsgReportSchema, MsgFeedSchema,
];
