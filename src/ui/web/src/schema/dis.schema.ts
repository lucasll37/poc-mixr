import type { FactorySchema } from './types';

export const UdpBroadcastHandlerSchema: FactorySchema = {
  factoryName: 'UdpBroadcastHandler',
  baseClass: 'mixr::base::NetHandler',
  group: 'dis',
  label: 'Handler de rede UDP',
  slots: [
    { name: 'localIpAddress', valueShape: 'identifier', default: 'localhost' },
    { name: 'networkMask', valueShape: 'string', default: '255.0.0.0' },
    { name: 'port', valueShape: 'number', required: true, note: 'todo processo escuta na MESMA porta (convencao do repo: 3000)' },
    { name: 'localPort', valueShape: 'number', note: 'porta de EMISSAO deste processo -- unica por processo' },
    { name: 'ignoreSourcePort', valueShape: 'number', note: 'igual ao proprio localPort, pra nao ouvir o proprio eco' },
    { name: 'shared', valueShape: 'boolean', default: true },
  ],
  insertionPoints: [
    { parentFactory: 'DisNetIO', parentSlot: 'netInput', shape: 'form' },
    { parentFactory: 'DisNetIO', parentSlot: 'netOutput', shape: 'form' },
  ],
};

export const DisNtmSchema: FactorySchema = {
  factoryName: 'DisNtm',
  baseClass: 'mixr::interop::common::Ntm',
  group: 'dis',
  label: 'Casamento de entidade DIS (DisNtm)',
  slots: [
    {
      name: 'disEntityType',
      valueShape: 'rawVector',
      required: true,
      note: 'vetor [kind domain country category subcategory specific extra] -- precisa ser IDENTICO nos dois lados de um par emissor/receptor',
    },
    { name: 'template', valueShape: 'form', required: true, allowedFactories: ['Aircraft'],
      note: 'o "molde" clonado quando chega o primeiro PDU dessa entidade -- dynamicsModel/pilot do lado receptor sao irrelevantes (so dead reckoning)' },
  ],
  insertionPoints: [
    { parentFactory: 'DisNetIO', parentSlot: 'inputEntityTypes', shape: 'positionalList' },
    { parentFactory: 'DisNetIO', parentSlot: 'outputEntityTypes', shape: 'positionalList' },
  ],
};

export const DisNetIOSchema: FactorySchema = {
  factoryName: 'DisNetIO',
  baseClass: 'mixr::interop::dis::NetIO',
  group: 'dis',
  label: 'Rede DIS (DisNetIO)',
  requiresNativeFactories: ['mixr::dis::factory'],
  slots: [
    { name: 'siteID', valueShape: 'number', required: true, default: 1 },
    { name: 'applicationID', valueShape: 'number', required: true, default: 1 },
    { name: 'exerciseID', valueShape: 'number', required: true, default: 1 },
    { name: 'netInput', valueShape: 'form', required: true, allowedFactories: ['UdpBroadcastHandler'] },
    { name: 'netOutput', valueShape: 'form', required: true, allowedFactories: ['UdpBroadcastHandler'],
      note: 'obrigatorio mesmo num DisNetIO so-receptor -- initNetwork() inicializa os dois incondicionalmente' },
    { name: 'outputEntityTypes', valueShape: 'positionalList', allowedFactories: ['DisNtm'] },
    { name: 'inputEntityTypes', valueShape: 'positionalList', allowedFactories: ['DisNtm'] },
  ],
  insertionPoints: [{ parentFactory: 'ClockStation', parentSlot: 'networks', shape: 'positionalList' }],
};

export const disSchemas: FactorySchema[] = [UdpBroadcastHandlerSchema, DisNtmSchema, DisNetIOSchema];
