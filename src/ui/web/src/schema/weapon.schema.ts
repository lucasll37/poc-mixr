import type { FactorySchema } from './types';

// GuidedMissile: EMPTY_SLOTTABLE, sem slots proprios -- tudo herdado de
// Missile/AbstractWeapon/Player (models/missile/src/xmissile/GuidedMissile.hpp:17-19).
// Vem de um plugin SEPARADO (libmissile.so), nao do mesmo .so do flight --
// ver requiresPluginProvides.
export const GuidedMissileSchema: FactorySchema = {
  factoryName: 'GuidedMissile',
  baseClass: 'mixr::models::Missile',
  group: 'weapon',
  label: 'Missil guiado (GuidedMissile)',
  requiresPluginProvides: ['GuidedMissile'],
  slots: [
    { name: 'id', valueShape: 'number', required: true },
    { name: 'side', valueShape: 'identifier', required: true, default: 'blue' },
    { name: 'type', valueShape: 'string', required: true, default: 'AIM1' },
    { name: 'signature', valueShape: 'form', allowedFactories: ['SigSphere'] },
    { name: 'dataLogTime', valueShape: 'unitNumber', unitFamily: 'Time', allowedUnits: ['Seconds'], default: 0.1 },
    { name: 'maxTOF', valueShape: 'unitNumber', unitFamily: 'Time', allowedUnits: ['Seconds'], default: 45,
      note: 'tempo maximo de voo antes de autodetonar' },
    { name: 'lethalRange', valueShape: 'unitNumber', unitFamily: 'Distance', allowedUnits: ['Meters'], default: 30 },
    { name: 'maxBurstRng', valueShape: 'unitNumber', unitFamily: 'Distance', allowedUnits: ['Meters'], default: 150 },
    {
      name: 'components',
      valueShape: 'namedList',
      allowedFactories: ['JSBSimModel'],
      note: 'so dynamicsModel: (JSBSimModel model:"aim1") -- o missil voa guiado pela dinamica JSBSim, nao pela cinematica simplificada de Missile',
    },
  ],
  insertionPoints: [{ parentFactory: 'StoresMgr', parentSlot: 'stores', shape: 'namedList' }],
};

export const weaponSchemas: FactorySchema[] = [GuidedMissileSchema];
