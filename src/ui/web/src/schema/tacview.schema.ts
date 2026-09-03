import type { FactorySchema } from './types';

export const TacviewOutputSchema: FactorySchema = {
  factoryName: 'TacviewOutput',
  baseClass: 'mixr::xtacview::TacviewOutput',
  group: 'tacview',
  label: 'Saida Tacview (TacviewOutput)',
  slots: [
    { name: 'host', valueShape: 'string', default: '0.0.0.0' },
    { name: 'port', valueShape: 'number', required: true, default: 1234 },
    { name: 'fileName', valueShape: 'string', required: true },
    { name: 'callsign', valueShape: 'string', required: true },
    { name: 'typeMap', valueShape: 'namedList', note: 'chave = nome/type do player, valor = Type= do ACMI' },
    { name: 'colorMap', valueShape: 'namedList', note: 'chave = nome/side do player, valor = Color= do ACMI' },
    { name: 'modelMap', valueShape: 'namedList', note: 'chave = nome do player, valor = Name= do ACMI' },
  ],
  insertionPoints: [{ parentFactory: 'RecorderOutputHandler', parentSlot: 'components', shape: 'positionalList' }],
};

export const RecorderOutputHandlerSchema: FactorySchema = {
  factoryName: 'RecorderOutputHandler',
  baseClass: 'mixr::recorder::OutputHandler',
  group: 'tacview',
  label: 'Cadeia de saida do gravador',
  slots: [{ name: 'components', valueShape: 'positionalList', allowedFactories: ['TacviewOutput'] }],
  insertionPoints: [{ parentFactory: 'ExposedDataRecorder', parentSlot: 'outputHandler', shape: 'form' }],
};

export const ExposedDataRecorderSchema: FactorySchema = {
  factoryName: 'ExposedDataRecorder',
  baseClass: 'mixr::recorder::DataRecorder',
  group: 'tacview',
  label: 'Gravador de dados (ExposedDataRecorder)',
  slots: [
    { name: 'eventName', valueShape: 'string' },
    {
      name: 'enabledList',
      valueShape: 'rawVector',
      note: 'so [43 42] (REID_PLAYER_DATA/REID_PLAYER_REMOVED) -- NUNCA incluir REID de weapon-release (61): aborta o processo, bug nativo confirmado',
    },
    { name: 'outputHandler', valueShape: 'form', required: true, allowedFactories: ['RecorderOutputHandler'] },
  ],
  insertionPoints: [{ parentFactory: 'ClockStation', parentSlot: 'dataRecorder', shape: 'form' }],
};

export const tacviewSchemas: FactorySchema[] = [
  TacviewOutputSchema, RecorderOutputHandlerSchema, ExposedDataRecorderSchema,
];
