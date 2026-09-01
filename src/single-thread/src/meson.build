# include/ próprio deste subprojeto (app/, domain/, xnative/, ubf/, bt/)
local_inc_dir = include_directories('../include')

# ==============================================================================
# DOIS ALVOS: o HOST e o MODELO.
#
# O executável é a aplicação — o laço, o cenário, a Station, a exportação e o
# dump. O MODELO (domain/, bt/, ubf/, xnative/) não é linkado nele: vira um
# shared_module aberto com dlopen durante o parse do .epp, declarado no bloco
# ( PluginLoader ) do cenário.
#
# Trocar uma regra de negócio passa a recompilar um .so de segundos, e não a
# aplicação inteira. Ver shared/xplugin/README.md.
#
# A ÚNICA coisa que os dois lados compartilham é o quadro de leitura
# (shared/xboard), que é shared_library() de verdade justamente por isso.
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

    # Consultas PURAS sobre tipos nativos do MIXR (Track/TrackManager, Gimbal).
    # Moram em xnative/ por vizinhança, mas não são modelo: não incluem nada de
    # domain/, bt/ ou ubf/. Ficam no host porque o dump e a exportação para o
    # Tacview precisam delas.
    #
    # TrackQuery.cpp entra TAMBÉM no modelo, logo abaixo. São funções sem
    # estado (nenhum static, nenhum global), então as duas cópias não têm como
    # divergir — é o preço, pequeno e explícito, de não criar uma terceira .so
    # só para duas funções.
    'xnative/TrackQuery.cpp',
    'xnative/RadarScan.cpp',
)

model_sources = files(
    # a fronteira C do plugin (byte-idêntica nas duas pocs)
    'plugin.cpp',

    # regras de negócio puras (sem MIXR, sem BehaviorTree.CPP)
    'domain/geometry.cpp',
    'domain/PatrolPlan.cpp',
    'domain/RtbPlan.cpp',
    'domain/TerrainFloor.cpp',
    'domain/ThreatPolicy.cpp',

    # o pouco que continua sendo nosso do lado MIXR
    'xnative/AlertDatalink.cpp',
    'xnative/TacticalAlert.cpp',
    'xnative/factory.cpp',

    # utilitários de runtime, um por questão
    'xnative/ThreadTag.cpp',

    # ver o comentário em host_sources: mesma fonte, compilada dos dois lados
    'xnative/TrackQuery.cpp',

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

# ------------------------------------------------------------------------------
# O HOST
#
# Sem jsbsim_dep: aqui quem fala com o JSBSim é o JSBSimModel do framework.
#
# E sem behavior_tree_dep, o que é NOVO e deliberado: a BehaviorTree.CPP deste
# pacote Conan é ESTÁTICA (libbehaviortree_cpp_v3.a — não há .so). Linkada dos
# dois lados, o processo ficaria com duas cópias dos estáticos dela — entre
# eles o contador de BT::getUID(), o que daria UIDs de nó duplicados através da
# fronteira. Nada em app/ toca BT.CPP, então o host simplesmente a larga.
#
# "single-thread" é só ONDE a decisão do UBF roda. A SIMULAÇÃO é multithread nas duas
# (numTcThreads no .epp, players espalhados pelo pool de tempo crítico); o
# binário passa no determinismo com 1, 2 e 4 threads.
# ------------------------------------------------------------------------------
single_thread = executable('single-thread',
    host_sources,
    cpp_args            : asan_cpp_args,
    link_args           : asan_link_args + rpath_link_args,
    include_directories : local_inc_dir,
    dependencies        : [thread_dep, mixr_dep, xtacview_dep, xclock_dep, xjoystick_dep, xlog_dep, xboard_dep, xplugin_dep, xmsg_dep],
    install             : true,
    install_dir         : get_option('bindir'),
    # own_libs_rpath ($ORIGIN/../lib) porque xlog e xboard sao .so NOSSAS e
    # instalam em dist/lib, enquanto o binario vai para dist/bin.
    install_rpath       : mixr_libdir + ':' + own_libs_rpath,
    build_rpath         : mixr_libdir,
)

# ------------------------------------------------------------------------------
# O MODELO, como plugin
#
# shared_module() e não library(): artefato NÃO-LINKÁVEL, o que impede
# estruturalmente que alguém o ponha num link_with: e acabe com duas cópias dos
# MetaObject das nossas classes no processo.
#
# -Wl,--exclude-libs,ALL é OBRIGATÓRIO aqui, e não é otimização. A
# BehaviorTree.CPP é estática e traz 447 símbolos T globais; o
# gnu_symbol_visibility:'hidden' NÃO se aplica a objetos vindos de um arquivo
# .a, então sem esta linha eles vazariam para o .dynsym do plugin. Dois
# plugins poderiam se interpor, e tests/plugin/check_plugin_symbol.sh ficaria
# vermelho — com razão.
#
# Depende de xboard_dep e xlog_dep, que são as ÚNICAS libs de shared/ que um
# plugin pode linkar: as duas são shared_library() de verdade. As outras cinco
# são static_library(), e linká-las daria ao .so uma cópia privada dos
# estáticos delas (ver shared/xplugin/README.md).
# ------------------------------------------------------------------------------
single_thread_model = shared_module('single_thread_model',
    model_sources,
    cpp_args            : asan_cpp_args,
    link_args           : asan_link_args + rpath_link_args
                          + ['-Wl,--no-undefined', '-Wl,--exclude-libs,ALL'],
    include_directories : local_inc_dir,
    dependencies        : [thread_dep, mixr_dep, behavior_tree_dep, xlog_dep, xboard_dep, xplugin_abi_dep],
    gnu_symbol_visibility : 'hidden',
    install             : true,
    install_dir         : get_option('libdir') / 'mixr-plugins',
    # instalado em dist/lib/mixr-plugins/, entao '$ORIGIN/..' alcanca dist/lib,
    # onde ficam a libxlog.so e a libxboard.so.
    install_rpath       : mixr_libdir + ':$ORIGIN/..',
    build_rpath         : mixr_libdir,
)
