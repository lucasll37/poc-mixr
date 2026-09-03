import type { FactorySchema } from './types';

export const PluginModuleSchema: FactorySchema = {
  factoryName: 'PluginModule',
  baseClass: 'mixr::xplugin::PluginModule',
  group: 'plugin',
  label: 'Modulo de plugin (PluginModule)',
  slots: [
    { name: 'file', valueShape: 'string', required: true, note: 'so o nome do arquivo .so -- resolvido contra searchPaths' },
    {
      name: 'provides',
      valueShape: 'positionalList',
      required: true,
      note: 'ASSERCAO: precisa casar EXATAMENTE com o que o .so registra -- mixrFactory aborta explicando a diferenca se nao bater',
    },
  ],
  insertionPoints: [{ parentFactory: 'PluginLoader', parentSlot: 'modules', shape: 'positionalList' }],
};

export const PluginLoaderSchema: FactorySchema = {
  factoryName: 'PluginLoader',
  baseClass: 'mixr::xplugin::PluginLoader',
  group: 'plugin',
  label: 'Carregador de plugins (PluginLoader)',
  slots: [
    { name: 'searchPaths', valueShape: 'positionalList', required: true, default: './dist/lib/mixr-plugins/' },
    { name: 'modules', valueShape: 'positionalList', allowedFactories: ['PluginModule'], required: true },
  ],
  insertionPoints: [{ parentFactory: 'ClockStation', parentSlot: 'components', shape: 'namedList' }],
};

export const pluginSchemas: FactorySchema[] = [PluginModuleSchema, PluginLoaderSchema];

/**
 * Presets reais do repo -- usados pelo botao de correcao rapida do inspector
 * quando um node exige um PluginModule ausente (ver components/palette).
 */
export const KNOWN_PLUGIN_PRESETS = [
  {
    file: 'libflight.so',
    provides: ['AlertDatalink', 'TacticalAlert', 'FlightState', 'BtBehavior', 'AltitudeSafetyBehavior', 'FlightAction'],
    note: 'topologia single-thread (SimAgent) -- 6 nomes, sem FlightAgentTC',
  },
  {
    file: 'libflight_tc.so',
    provides: [
      'AlertDatalink', 'TacticalAlert', 'FlightAgentTC', 'FlightState',
      'BtBehavior', 'AltitudeSafetyBehavior', 'FlightAction',
    ],
    note: 'topologia multi-thread (FlightAgentTC) -- os mesmos 6 + FlightAgentTC',
  },
  {
    file: 'libmissile.so',
    provides: ['GuidedMissile'],
    note: 'plugin separado, so o missil',
  },
] as const;
