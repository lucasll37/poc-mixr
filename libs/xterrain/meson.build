# Terreno multi-tile (SRTM), com carga sob demanda e cache LRU -- alternativa
# ao SrtmHgtFile nativo (que so serve UM tile) para cenarios cuja area de voo
# excede 1x1 grau. Mesmo padrao 'libs/x<nome>' de libs/xtacview/xclock/
# xjoystick (factory propria + classe em mixr::xterrain) -- estatica, nao
# shared_library(): nenhum modelo/plugin toca o tipo CONCRETO de Terrain (so
# o Player/WorldView generico ja expoe AGL/elevacao ao domain/), entao nao ha
# estado que precise atravessar a fronteira de dlopen.
#
# xlog_dep: os avisos de diretorio ausente/tile invalido usam LOG(...) em vez
# de stderr cru -- mesmo motivo ja documentado em libs/xtacview.

xterrain_inc_dir = include_directories('..')

xterrain_lib = static_library('xterrain',
    files(
        'MultiTileTerrain.cpp',
        'factory.cpp',
    ),
    include_directories : xterrain_inc_dir,
    dependencies        : [mixr_dep, xlog_dep],
)

xterrain_dep = declare_dependency(
    link_with           : xterrain_lib,
    include_directories : xterrain_inc_dir,
    dependencies        : [mixr_dep, xlog_dep],
)
