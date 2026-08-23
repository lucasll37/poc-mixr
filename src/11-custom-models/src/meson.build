# include/ próprio deste subprojeto (domain/, xdrone/, bt/), além do raiz
local_inc_dir = include_directories('../include')

sources = files(
    'main.cpp',
    'mixr_factory.cpp',

    # regras de negócio puras (sem MIXR, sem BehaviorTree.CPP)
    'domain/geometry.cpp',
    'domain/PatrolPlan.cpp',
    'domain/ThreatPolicy.cpp',

    # classes MIXR próprias desta poc (player, dinâmica, subsistemas)
    'xdrone/Drone.cpp',
    'xdrone/DroneDynamics.cpp',
    'xdrone/FuelSystem.cpp',
    'xdrone/ProximitySensor.cpp',
    'xdrone/BtPilot.cpp',
    'xdrone/factory.cpp',
    'xdrone/runtime_utils.cpp',

    # nós/factory da árvore de comportamento
    'bt/bt_factory.cpp',
    'bt/nodes/ContactDetectedCondition.cpp',
    'bt/nodes/EvadeAction.cpp',
    'bt/nodes/FuelLowCondition.cpp',
    'bt/nodes/PatrolAction.cpp',
    'bt/nodes/ReturnToBaseAction.cpp',
)

custom_models = executable('custom-models',
    sources,
    include_directories : [inc_dir, local_inc_dir],
    dependencies        : [thread_dep, mixr_dep, behavior_tree_dep, xtacview_dep],
    install             : true,
    install_dir         : get_option('bindir'),
    install_rpath       : mixr_bt_libdir,
    build_rpath         : mixr_bt_libdir,
    link_args           : rpath_link_args,
)
