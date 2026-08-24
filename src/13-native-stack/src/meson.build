# include/ próprio deste subprojeto (domain/, xnative/, ubf/, bt/)
local_inc_dir = include_directories('../include')

sources = files(
    'main.cpp',
    'mixr_factory.cpp',

    # regras de negócio puras (idênticas às da poc/12)
    'domain/geometry.cpp',
    'domain/PatrolPlan.cpp',
    'domain/ThreatPolicy.cpp',

    # o pouco que continua sendo nosso do lado MIXR
    'xnative/AlertDatalink.cpp',
    'xnative/TacticalAlert.cpp',
    'xnative/factory.cpp',
    'xnative/runtime_utils.cpp',

    # UBF: percepção, decisão (árvore dentro), atuação
    'ubf/FlightState.cpp',
    'ubf/FlightAction.cpp',
    'ubf/BtBehavior.cpp',
    'ubf/AltitudeSafetyBehavior.cpp',

    # nós/factory da árvore de comportamento
    'bt/bt_factory.cpp',
    'bt/nodes/AlertReceivedCondition.cpp',
    'bt/nodes/ContactDetectedCondition.cpp',
    'bt/nodes/FuelLowCondition.cpp',
    'bt/nodes/PatrolAction.cpp',
    'bt/nodes/ReportAndEvadeAction.cpp',
    'bt/nodes/ReturnToBaseAction.cpp',
    'bt/nodes/SupportAlertAction.cpp',
)

# Sem jsbsim_dep: aqui quem fala com o JSBSim é o JSBSimModel do framework.
native_stack = executable('native-stack',
    sources,
    cpp_args            : asan_cpp_args,
    link_args           : asan_link_args + rpath_link_args,
    include_directories : [inc_dir, local_inc_dir],
    dependencies        : [thread_dep, mixr_dep, behavior_tree_dep, xtacview_dep, xclock_dep],
    install             : true,
    install_dir         : get_option('bindir'),
    install_rpath       : mixr_bt_libdir,
    build_rpath         : mixr_bt_libdir,
)
