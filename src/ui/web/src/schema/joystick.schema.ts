import type { FactorySchema } from './types';

export const AnalogInputSchema: FactorySchema = {
  factoryName: 'AnalogInput',
  baseClass: 'mixr::linkage::AnalogInput',
  group: 'joystick',
  label: 'Canal analogico (AnalogInput)',
  requiresNativeFactories: ['mixr::linkage::factory'],
  slots: [
    { name: 'ai', valueShape: 'number', required: true, note: 'canal LOGICO do IoData -- 1-based (ROLL_AI=1, PITCH_AI=2, RUDDER_AI=3, THROTTLE_AI=4)' },
    { name: 'channel', valueShape: 'number', required: true, note: 'canal FISICO do dispositivo -- 0-based' },
    { name: 'deadband', valueShape: 'number', default: 0 },
    { name: 'offset', valueShape: 'number', default: 0 },
    { name: 'gain', valueShape: 'number', default: 1 },
  ],
  insertionPoints: [{ parentFactory: 'UsbJoystick', parentSlot: 'adapters', shape: 'positionalList' }],
};

export const UsbJoystickSchema: FactorySchema = {
  factoryName: 'UsbJoystick',
  baseClass: 'mixr::linkage::UsbJoystick',
  group: 'joystick',
  label: 'Joystick USB (UsbJoystick)',
  requiresNativeFactories: ['mixr::linkage::factory'],
  slots: [
    { name: 'deviceIndex', valueShape: 'number', default: 0 },
    { name: 'adapters', valueShape: 'positionalList', allowedFactories: ['AnalogInput'] },
  ],
  insertionPoints: [{ parentFactory: 'JoystickIoHandler', parentSlot: 'devices', shape: 'positionalList' }],
};

export const JoystickIoHandlerSchema: FactorySchema = {
  factoryName: 'JoystickIoHandler',
  baseClass: 'mixr::linkage::IoHandler',
  group: 'joystick',
  label: 'Handler de joystick (JoystickIoHandler)',
  requiresNativeFactories: ['mixr::linkage::factory'],
  slots: [
    { name: 'player', valueShape: 'playerRef', required: true },
    { name: 'deviceIndex', valueShape: 'number', default: 0 },
    { name: 'devices', valueShape: 'positionalList', allowedFactories: ['UsbJoystick'] },
  ],
  insertionPoints: [{ parentFactory: 'Station', parentSlot: 'ioHandler', shape: 'form' }],
};

export const joystickSchemas: FactorySchema[] = [AnalogInputSchema, UsbJoystickSchema, JoystickIoHandlerSchema];
