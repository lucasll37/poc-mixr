import type { FactorySchema } from './types';

// BtBehavior: models/flight/src/ubf/BtBehaviorSlots.cpp -- fornecido pelo
// plugin (libflight.so/libflight_tc.so), nao nativo do framework.
export const BtBehaviorSchema: FactorySchema = {
  factoryName: 'BtBehavior',
  baseClass: 'mixr::base::ubf::AbstractBehavior',
  group: 'ubf',
  label: 'Comportamento por arvore (BtBehavior)',
  requiresPluginProvides: ['BtBehavior'],
  slots: [
    { name: 'vote', valueShape: 'number', required: true, default: 50, note: 'prioridade entre comportamentos do mesmo UbfArbiter' },
    { name: 'treeFile', valueShape: 'string', required: true, default: './dist/share/mixr-plugins/flight/flight_tree.xml' },
    { name: 'patrolHeading', valueShape: 'unitNumber', unitFamily: 'Angle', allowedUnits: ['Degrees'] },
    { name: 'legTime', valueShape: 'unitNumber', unitFamily: 'Time', allowedUnits: ['Seconds'], default: 60 },
    { name: 'legTurn', valueShape: 'unitNumber', unitFamily: 'Angle', allowedUnits: ['Degrees'], default: 90 },
    { name: 'patrolAltitude', valueShape: 'unitNumber', unitFamily: 'Distance', allowedUnits: ['Meters'], default: 4000 },
    { name: 'patrolSpeed', valueShape: 'number', default: 350, note: 'nos (knots)' },
    { name: 'rtbAltitude', valueShape: 'unitNumber', unitFamily: 'Distance', allowedUnits: ['Meters'], default: 3000 },
    { name: 'rtbSpeed', valueShape: 'number', default: 400 },
    { name: 'arrivalRadius', valueShape: 'unitNumber', unitFamily: 'Distance', allowedUnits: ['NauticalMiles'], default: 3 },
    { name: 'fuelReserve', valueShape: 'number', default: 0.35, note: 'fracao 0..1 -- aciona RTB' },
    { name: 'breakTurn', valueShape: 'unitNumber', unitFamily: 'Angle', allowedUnits: ['Degrees'], default: 110 },
    { name: 'evadeClimb', valueShape: 'unitNumber', unitFamily: 'Distance', allowedUnits: ['Meters'], default: 600 },
    { name: 'evadeSpeed', valueShape: 'number', default: 450 },
    { name: 'supportSpeed', valueShape: 'number', default: 420 },
    { name: 'evadeHold', valueShape: 'unitNumber', unitFamily: 'Time', allowedUnits: ['Seconds'], default: 30 },
    { name: 'terrainClearance', valueShape: 'unitNumber', unitFamily: 'Distance', allowedUnits: ['Meters'], default: 500,
      note: '0 desabilita o piso anti-CFIT' },
    { name: 'launchMinRange', valueShape: 'unitNumber', unitFamily: 'Distance', allowedUnits: ['NauticalMiles'], default: 0.8 },
    { name: 'launchMaxRange', valueShape: 'unitNumber', unitFamily: 'Distance', allowedUnits: ['NauticalMiles'], default: 20,
      note: 'tem de cobrir o alcance do PRIMEIRO contato -- ver armadilha no CLAUDE.md ("Demo: missil guiado")' },
    { name: 'launchCone', valueShape: 'unitNumber', unitFamily: 'Angle', allowedUnits: ['Degrees'], default: 45 },
  ],
  insertionPoints: [{ parentFactory: 'UbfArbiter', parentSlot: 'behaviors', shape: 'positionalList' }],
};

export const AltitudeSafetyBehaviorSchema: FactorySchema = {
  factoryName: 'AltitudeSafetyBehavior',
  baseClass: 'mixr::base::ubf::AbstractBehavior',
  group: 'ubf',
  label: 'Piso de seguranca (AltitudeSafetyBehavior)',
  requiresPluginProvides: ['AltitudeSafetyBehavior'],
  slots: [
    { name: 'vote', valueShape: 'number', required: true, default: 90 },
    { name: 'minAltitude', valueShape: 'unitNumber', unitFamily: 'Distance', allowedUnits: ['Meters'], required: true },
    { name: 'recoverAltitude', valueShape: 'unitNumber', unitFamily: 'Distance', allowedUnits: ['Meters'], required: true },
    { name: 'recoverSpeed', valueShape: 'number', required: true },
    { name: 'minClearance', valueShape: 'unitNumber', unitFamily: 'Distance', allowedUnits: ['Meters'],
      note: 'piso AGL -- tem de ficar ABAIXO do terrainClearance do BtBehavior, senao os dois brigam' },
    { name: 'recoverClearance', valueShape: 'unitNumber', unitFamily: 'Distance', allowedUnits: ['Meters'] },
  ],
  insertionPoints: [{ parentFactory: 'UbfArbiter', parentSlot: 'behaviors', shape: 'positionalList' }],
};

export const FlightStateSchema: FactorySchema = {
  factoryName: 'FlightState',
  baseClass: 'mixr::base::ubf::AbstractState',
  group: 'ubf',
  label: 'Percepcao (FlightState)',
  requiresPluginProvides: ['FlightState'],
  slots: [],
  insertionPoints: [
    { parentFactory: 'SimAgent', parentSlot: 'state', shape: 'form' },
    { parentFactory: 'FlightAgentTC', parentSlot: 'state', shape: 'form' },
  ],
};

export const AlertDatalinkSchema: FactorySchema = {
  factoryName: 'AlertDatalink',
  baseClass: 'mixr::models::Datalink',
  group: 'ubf',
  label: 'Datalink de alerta (AlertDatalink)',
  requiresPluginProvides: ['AlertDatalink'],
  slots: [{ name: 'holdTime', valueShape: 'unitNumber', unitFamily: 'Time', allowedUnits: ['Seconds'], default: 25 }],
  insertionPoints: [{ parentFactory: 'Aircraft', parentSlot: 'components', shape: 'namedList' }],
};

export const UbfArbiterSchema: FactorySchema = {
  factoryName: 'UbfArbiter',
  baseClass: 'mixr::base::ubf::Arbiter',
  group: 'ubf',
  label: 'Arbitro de comportamentos (UbfArbiter)',
  slots: [
    {
      name: 'behaviors',
      valueShape: 'positionalList',
      allowedFactories: ['BtBehavior', 'AltitudeSafetyBehavior'],
      required: true,
    },
  ],
  insertionPoints: [
    { parentFactory: 'SimAgent', parentSlot: 'behavior', shape: 'form' },
    { parentFactory: 'FlightAgentTC', parentSlot: 'behavior', shape: 'form' },
  ],
};

// SimAgent (nativo mixr::models::SimAgent) -- topologia single-thread: vive
// em components: da PROPRIA STATION (nao dentro de players:), amarrado ao
// ator por nome. Confirmado lendo src/poc/single-thread/configs/scenario.epp.in:36-82.
export const SimAgentSchema: FactorySchema = {
  factoryName: 'SimAgent',
  baseClass: 'mixr::models::SimAgent',
  group: 'ubf',
  label: 'Agente de decisao (SimAgent, single-thread)',
  slots: [
    { name: 'actorPlayerName', valueShape: 'playerRef', required: true },
    { name: 'state', valueShape: 'form', allowedFactories: ['FlightState'], required: true },
    { name: 'behavior', valueShape: 'form', allowedFactories: ['UbfArbiter'], required: true },
  ],
  insertionPoints: [{ parentFactory: 'ClockStation', parentSlot: 'components', shape: 'namedList' }],
};

// FlightAgentTC (do plugin) -- topologia multi-thread: vive DENTRO de
// components: do proprio Player, como ULTIMO item (fase 3 do frame T/C, ve
// as pistas ja atualizadas deste frame). Confirmado em
// app/configs/scenario_intercept_missile.epp.in:141-183.
export const FlightAgentTCSchema: FactorySchema = {
  factoryName: 'FlightAgentTC',
  baseClass: 'mixr::base::ubf::AgentTC',
  group: 'ubf',
  label: 'Agente de decisao (FlightAgentTC, multi-thread)',
  requiresPluginProvides: ['FlightAgentTC'],
  slots: [
    { name: 'state', valueShape: 'form', allowedFactories: ['FlightState'], required: true },
    { name: 'behavior', valueShape: 'form', allowedFactories: ['UbfArbiter'], required: true },
  ],
  insertionPoints: [{ parentFactory: 'Aircraft', parentSlot: 'components', shape: 'namedList' }],
};

export const ubfSchemas: FactorySchema[] = [
  BtBehaviorSchema, AltitudeSafetyBehaviorSchema, FlightStateSchema,
  AlertDatalinkSchema, UbfArbiterSchema, SimAgentSchema, FlightAgentTCSchema,
];
