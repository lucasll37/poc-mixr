// O catalogo de schema e a "fonte da verdade" que substitui a introspecao
// C++ que nao existe (a verdade real dos slots esta espalhada pelas macros
// BEGIN_SLOTTABLE/BEGIN_SLOT_MAP de cada classe do framework/plugin). Toda
// entrada aqui foi conferida contra pelo menos um .epp/.epp.in REAL do repo
// -- nunca inventada so a partir de documentacao C++ isolada.

export type SlotValueShape =
  | 'number'
  | 'unitNumber'
  | 'string'
  | 'identifier'
  | 'boolean'
  | 'rawVector'
  | 'playerRef'
  | 'positionalList'
  | 'namedList'
  | 'form';

export type UnitFamily = 'Distance' | 'Angle' | 'Time' | 'Frequency' | 'Power' | 'Ratio';

export const UNIT_FAMILIES: Record<UnitFamily, string[]> = {
  Distance: ['Meters', 'NauticalMiles', 'Feet', 'Kilometers'],
  Angle: ['Degrees', 'Radians'],
  Time: ['Seconds', 'MilliSeconds', 'MicroSeconds', 'Minutes', 'Hours'],
  Frequency: ['Hertz', 'KiloHertz', 'MegaHertz', 'GigaHertz'],
  Power: ['Watts', 'KiloWatts', 'dB', 'DecibelWatts'],
  Ratio: ['dB', 'Decibel'],
};

export interface SlotSchema {
  name: string;
  valueShape: SlotValueShape;
  unitFamily?: UnitFamily;
  allowedUnits?: string[];
  /** para 'form'/'positionalList'/'namedList': quais factoryName podem entrar aqui */
  allowedFactories?: string[];
  required?: boolean;
  default?: number | string | boolean;
  /** PT-BR -- explica o "porque" quando nao e obvio; nao vira comentario automatico no EDL gerado. */
  note?: string;
}

export type FactoryGroup =
  | 'station'
  | 'player'
  | 'subsystem'
  | 'weapon'
  | 'ubf'
  | 'dis'
  | 'msg'
  | 'tacview'
  | 'joystick'
  | 'plugin';

export interface InsertionRule {
  /** nome de fabrica do PAI onde este node pode ser inserido. "*Aircraft" casa qualquer subclasse de Player. */
  parentFactory: string;
  parentSlot: string;
  shape: 'positionalList' | 'namedList' | 'form';
}

export interface FactorySchema {
  factoryName: string;
  baseClass?: string;
  group: FactoryGroup;
  /** rotulo curto em PT-BR para a paleta */
  label: string;
  slots: SlotSchema[];
  requiresNativeFactories?: string[];
  requiresPluginProvides?: string[];
  insertionPoints: InsertionRule[];
}
