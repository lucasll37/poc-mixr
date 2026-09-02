# So este alvo linka FTXUI -- declarado AQUI, nao no meson.build raiz, porque
# e o UNICO consumidor (ao contrario de mixr_dep/xtacview_dep, que varias
# pocs usam). Um unico dependency() basta: o ftxui.pc do Conan encadeia os
# tres componentes (screen -> dom -> component) via 'Requires:' (conferido em
# build/ftxui.pc depois do 'conan install').
ftxui_dep = dependency('ftxui', method: 'pkg-config', required: true)

local_inc_dir = include_directories('../include')

host_sources = files(
    'main.cpp', 'mixr_factory.cpp',
    'app/Options.cpp', 'app/TerrainData.cpp', 'app/ScenarioTemplate.cpp',
    'app/StationBuilder.cpp', 'app/Fleet.cpp', 'app/DeterministicDump.cpp',
    'app/DeterministicRun.cpp', 'app/MetaObjectReport.cpp',
    'app/ScenarioCatalog.cpp', 'app/Respawn.cpp', 'app/DashboardState.cpp',
    'app/ScenarioPickerScreen.cpp', 'app/DashboardLoop.cpp',
    'app/MetaObjectSnapshot.cpp', 'app/FleetPanel.cpp', 'app/MapPanel.cpp',
    'app/MemoryPanel.cpp', 'app/BehaviorTreeView.cpp',
)

# sem xjoystick_dep/xmsg_dep, de proposito -- ver mixr_factory.cpp: este poc
# nao encadeia mixr::xjoystick::factory()/mixr::xmsg::factory() (nenhum
# cenario de configs/ declara ioHandler:/msgFeed:).
dashboard = executable('dashboard',
    host_sources,
    cpp_args            : asan_cpp_args,
    link_args           : asan_link_args + rpath_link_args,
    include_directories : local_inc_dir,
    dependencies        : [thread_dep, mixr_dep, xtacview_dep, xclock_dep,
                           xlog_dep, xboard_dep, xtrack_dep, xplugin_dep, ftxui_dep],
    install             : true,
    install_dir         : get_option('bindir'),
    install_rpath       : mixr_libdir + ':' + own_libs_rpath,
    build_rpath         : mixr_libdir,
)
