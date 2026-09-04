# include/ próprio deste subprojeto — agora só app/ e o RadarScan
local_inc_dir = include_directories('../include')

# ==============================================================================
# O HOST, e SÓ o host.
#
# O MODELO (domain/, bt/, ubf/, xnative/) não está aqui e este arquivo não sabe
# onde ele mora: os fontes saíram para models/flight/, que é um PROJETO
# MESON SEPARADO, construído numa etapa ANTERIOR (`make models`). O host só
# consome o .so pronto, pelo bloco ( PluginLoader ) do cenário.
#
# É isso que torna verificável o cenário que motivou a separação: um terceiro
# entrega o binário e o host tem de funcionar sem nunca ter visto o fonte. Se
# alguma linha do modelo voltasse para cá, `tests/guard/check_host_opaco.sh`
# ficaria vermelho.
#
# Sem behavior_tree_dep: a BehaviorTree.CPP deste pacote Conan é ESTÁTICA, e
# quem decide (o modelo) é que a linka. Linkada dos dois lados, o processo
# ficaria com duas cópias dos estáticos dela.
# ==============================================================================

host_sources = files(
    # main.cpp só orquestra; cada etapa vive num módulo de app/
    'main.cpp',
    'mixr_factory.cpp',

    # aplicação: linha de comando, cenário, Station, players e os dois laços
    'app/Options.cpp',
    'app/TerrainData.cpp',
    'app/ScenarioTemplate.cpp',
    'app/StationBuilder.cpp',
    'app/Fleet.cpp',
    'app/StatusReport.cpp',
    'app/DeterministicDump.cpp',
    'app/DeterministicRun.cpp',
    'app/MetaObjectReport.cpp',
    'app/RealTimeRun.cpp',
    'app/Shutdown.cpp',
)

single_thread = executable('single-thread',
    host_sources,
    cpp_args            : asan_cpp_args,
    link_args           : asan_link_args + rpath_link_args,
    include_directories : local_inc_dir,
    dependencies        : [thread_dep, mixr_dep, xtacview_dep, xclock_dep, xjoystick_dep,
                           xlog_dep, xboard_dep, xtrack_dep, xplugin_dep, xmsg_dep],
    install             : true,
    install_dir         : get_option('bindir'),
    # own_libs_rpath ($ORIGIN/../lib) porque xlog, xboard e xtrack são .so
    # NOSSAS e instalam em dist/lib, enquanto o binário vai para dist/bin.
    install_rpath       : mixr_libdir + ':' + own_libs_rpath,
    build_rpath         : mixr_libdir,
)
