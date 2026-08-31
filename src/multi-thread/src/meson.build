# include/ próprio deste subprojeto (app/, domain/, xnative/, ubf/, bt/)
local_inc_dir = include_directories('../include')

sources = files(
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

    # regras de negócio puras (sem MIXR, sem BehaviorTree.CPP)
    'domain/geometry.cpp',
    'domain/PatrolPlan.cpp',
    'domain/RtbPlan.cpp',
    'domain/TerrainFloor.cpp',
    'domain/ThreatPolicy.cpp',

    # o pouco que continua sendo nosso do lado MIXR
    'xnative/AlertDatalink.cpp',
    'xnative/FlightAgentTC.cpp',   # <- a ÚNICA classe a mais que o single-thread
    'xnative/TacticalAlert.cpp',
    'xnative/TrackQuery.cpp',
    'xnative/RadarScan.cpp',
    'xnative/factory.cpp',

    # utilitários de runtime, um por questão
    'xnative/ThreadTag.cpp',
    'xnative/BehaviorBoard.cpp',

    # UBF: percepção, decisão (árvore dentro), atuação
    'ubf/FlightState.cpp',
    'ubf/FlightAction.cpp',
    'ubf/BtBehavior.cpp',
    'ubf/BtBehaviorSlots.cpp',
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
#
# "multi-thread" é só ONDE a decisão do UBF roda: no ( FlightAgentTC )
# próprio, componente do player, que decide na fase 3 do frame — os 4
# agentes em paralelo, um por thread do pool de tempo crítico. A lista de
# fontes e as flags são as mesmas do single-thread, que já era
# multithread na simulação: o que muda é o agente, não o paralelismo do
# framework.
multi_thread = executable('multi-thread',
    sources,
    cpp_args            : asan_cpp_args,
    link_args           : asan_link_args + rpath_link_args,
    include_directories : local_inc_dir,
    dependencies        : [thread_dep, mixr_dep, behavior_tree_dep, xtacview_dep, xclock_dep, xjoystick_dep, xlog_dep, xmsg_dep],
    install             : true,
    install_dir         : get_option('bindir'),
    install_rpath       : mixr_bt_libdir,
    build_rpath         : mixr_bt_libdir,
)
