# include/ proprio deste subprojeto (formations.hpp, linkage/, bt/,
# tacview/, recorder_events.hpp) + a raiz do subprojeto (pra "configs/..."
# includes tipo channel_map.hpp usados a partir do C++, se precisar)
local_inc_dir = include_directories('../include')
subproject_root_inc_dir = include_directories('..')

sources = files(
    'main.cpp',
    'mixr_factory.cpp',
    'formations.cpp',
    'linkage/KeyboardDevice.cpp',
    'linkage/KeyboardIoHandler.cpp',
    'bt/bt_factory.cpp',
    'bt/nodes/FollowFormationAction.cpp',
    'bt/nodes/RtbEngagedCondition.cpp',
    'tacview/RealtimeTelemetryServer.cpp',
)

formation_flight = executable('formation-flight',
    sources,
    include_directories : [inc_dir, local_inc_dir, subproject_root_inc_dir],
    dependencies        : [thread_dep, mixr_dep, behavior_tree_dep],
    install             : true,
    install_dir         : get_option('bindir'),
    install_rpath       : mixr_libdir,
    build_rpath         : mixr_libdir,
    link_args           : ['-Wl,--disable-new-dtags'],
)
