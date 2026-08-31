# include/ próprio deste subprojeto (app/)
local_inc_dir = include_directories('../include')

sources = files(
    'main.cpp',
    'mixr_factory.cpp',

    'app/TerrainData.cpp',
    'app/StationBuilder.cpp',
    'app/RealTimeRun.cpp',
)

# Sem behavior_tree_dep: nao ha UBF/BehaviorTree aqui, so o bandit1 sozinho
# (joystick/Autopilot + emissao DIS). rpath tambem fica so com mixr_libdir,
# sem bt_libdir.
bandit_dis = executable('bandit-dis',
    sources,
    link_args           : rpath_link_args,
    include_directories : local_inc_dir,
    dependencies        : [thread_dep, mixr_dep, xtacview_dep, xjoystick_dep, xlog_dep, xmsg_dep],
    install             : true,
    install_dir         : get_option('bindir'),
    install_rpath       : mixr_libdir,
    build_rpath         : mixr_libdir,
)
