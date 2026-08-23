# include/ próprio deste subprojeto (domain/, xair/, ubf/, bt/)
local_inc_dir = include_directories('../include')

sources = files(
    'main.cpp',
    'mixr_factory.cpp',

    # regras de negócio puras (sem MIXR, sem BT, sem JSBSim)
    'domain/geometry.cpp',
    'domain/PatrolPlan.cpp',
    'domain/ThreatPolicy.cpp',

    # classes MIXR próprias: player, 6-DOF (JSBSim direto) e subsistemas
    'xair/Airplane.cpp',
    'xair/JsbsimFlightModel.cpp',
    'xair/FlightDirector.cpp',
    'xair/ProximitySensor.cpp',
    'xair/AlertRadio.cpp',
    'xair/TacticalAlert.cpp',
    'xair/factory.cpp',
    'xair/runtime_utils.cpp',

    # UBF: percepção, decisão (com a árvore dentro), atuação e o agente
    'ubf/FlightState.cpp',
    'ubf/FlightAction.cpp',
    'ubf/BtBehavior.cpp',
    'ubf/AltitudeSafetyBehavior.cpp',
    'ubf/FlightAgent.cpp',

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

jsbsim_ubf = executable('jsbsim-ubf',
    sources,
    include_directories : [inc_dir, local_inc_dir],
    dependencies        : [thread_dep, mixr_dep, behavior_tree_dep, xtacview_dep, jsbsim_dep],
    install             : true,
    install_dir         : get_option('bindir'),
    install_rpath       : mixr_bt_jsbsim_libdir,
    build_rpath         : mixr_bt_jsbsim_libdir,
    link_args           : rpath_link_args,
)
