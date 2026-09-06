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
    'app/MapGeometry.cpp', 'app/BreakpointController.cpp', 'app/SpeedLadder.cpp',
    'app/MemoryPanel.cpp', 'app/BehaviorTreeView.cpp', 'app/BackgroundPanel.cpp',
    'app/TerrainQuery.cpp', 'app/LogPanel.cpp', 'app/Shutdown.cpp',
    'app/ComponentTreeQuery.cpp', 'app/ComponentTreePanel.cpp',
    'app/ComponentFlowState.cpp', 'app/FrameCallChain.cpp',
    'app/EdlEditorState.cpp', 'app/ScenarioFolder.cpp',
)

# sem xjoystick_dep/xmsg_dep, de proposito -- ver mixr_factory.cpp: este poc
# nao encadeia mixr::xjoystick::factory()/mixr::xmsg::factory() (nenhum
# cenario de configs/ declara ioHandler:/msgFeed:).
app = executable('app',
    host_sources,
    cpp_args            : asan_cpp_args,
    link_args           : asan_link_args + rpath_link_args,
    include_directories : local_inc_dir,
    # xmsg_dep/xjoystick_dep entraram quando o ./app virou o runner unico das
    # pocs: os cenarios delas declaram ( MsgFeed ) e ( JoystickIoHandler ).
    dependencies        : [thread_dep, mixr_dep, xtacview_dep, xclock_dep, xjoystick_dep,
                           xlog_dep, xboard_dep, xtrack_dep, xplugin_dep, xmsg_dep, ftxui_dep],
    install             : true,
    install_dir         : get_option('bindir'),
    install_rpath       : mixr_libdir + ':' + own_libs_rpath,
    build_rpath         : mixr_libdir,
)

# Oraculo de validacao de .edl para o editor grafico -- ver o cabecalho de
# edlcheck_main.cpp para o porque de existir separado do 'app' (sem
# terreno/frota/WorldModel obrigatorios). Recompila mixr_factory.cpp de
# proposito (mesmo padrao ja usado por src/rl/bindings/meson.build -- nao
# ha lib estatica pronta pra isto no repositorio) para reusar a MESMA
# cadeia de factories de producao, byte a byte. xlog_dep/xboard_dep/
# xtrack_dep/ftxui_dep ficam de fora -- nenhum e tocado por mixr_factory.cpp
# nem por edlcheck_main.cpp, so pelo resto do 'app' (TUI).
edlcheck = executable('edlcheck',
    files('edlcheck_main.cpp', 'mixr_factory.cpp'),
    cpp_args            : asan_cpp_args,
    link_args           : asan_link_args + rpath_link_args,
    include_directories : local_inc_dir,
    dependencies        : [thread_dep, mixr_dep, xtacview_dep, xclock_dep, xjoystick_dep,
                           xplugin_dep, xmsg_dep],
    install             : true,
    install_dir         : get_option('bindir'),
    install_rpath       : mixr_libdir + ':' + own_libs_rpath,
    build_rpath         : mixr_libdir,
)
