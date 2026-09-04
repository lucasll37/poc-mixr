# ==============================================================================
# A suite do MODELO -- as duas camadas que testam regra de negocio.
#
# Vieram de tests/ do host quando os fontes saíram para ca: elas compilam
# domain/ e bt/ DIRETO, entao so fazem sentido onde esses fontes moram.
#
# ARMADILHA MEDIDA: 'meson test' devolve rc=0 para suite VAZIA ("No tests
# defined."), e o default de -Dtests e false. Sem uma contagem minima, perder
# estas duas camadas seria um verde silencioso -- o Makefile do host confere o
# numero com 'meson introspect --tests' (ver o alvo test-models).
# ==============================================================================
gtest_dep = dependency('gtest', method: 'pkg-config')
gtest_main_dep = dependency('gtest_main', method: 'pkg-config')

domain_sources = files(
    '../src/domain/geometry.cpp',
    '../src/domain/LaunchPolicy.cpp',
    '../src/domain/PatrolPlan.cpp',
    '../src/domain/RtbPlan.cpp',
    '../src/domain/TerrainFloor.cpp',
    '../src/domain/ThreatPolicy.cpp',
)

# Camada 1 -- regras puras: sem MIXR, sem BehaviorTree.CPP, sem rpath nenhum.
test_domain = executable('test-domain',
    files(
        'domain/test_geometry.cpp',
        'domain/test_LaunchPolicy.cpp',
        'domain/test_TerrainFloor.cpp',
        'domain/test_ThreatPolicy.cpp',
        'domain/test_PatrolPlan.cpp',
        'domain/test_RtbPlan.cpp',
    ) + domain_sources,
    include_directories : model_inc,
    dependencies        : [gtest_dep, gtest_main_dep],
    install             : false,
)
test('domain', test_domain, suite: 'domain', timeout: 60)

# Camada 2 -- a arvore de producao contra um contexto falso.
# Linka BehaviorTree.CPP; NAO linka MIXR -- e o que o refactor de
# bt/DecisionContext.hpp comprou, e continua valendo aqui.
bt_sources = files(
    '../src/bt/bt_factory.cpp',
    '../src/bt/nodes/AlertReceivedCondition.cpp',
    '../src/bt/nodes/ContactDetectedCondition.cpp',
    '../src/bt/nodes/FuelLowCondition.cpp',
    '../src/bt/nodes/LaunchEnvelopeCondition.cpp',
    '../src/bt/nodes/LaunchMissileAction.cpp',
    '../src/bt/nodes/PatrolAction.cpp',
    '../src/bt/nodes/ReportAndEvadeAction.cpp',
    '../src/bt/nodes/ReturnToBaseAction.cpp',
    '../src/bt/nodes/SupportAlertAction.cpp',
)

# Os nos que dependem do SDK ficam FORA de bt_sources, de proposito.
#
# bt_sources e compartilhado por test_tree e test_native, e test_tree tem uma
# propriedade que vale a pena preservar: ele NAO linka o MIXR (ver o comentario
# do alvo, logo abaixo) -- e o que o refactor de bt/DecisionContext.hpp
# comprou. Um no que chama shared/xinfer arrasta sdk_dep, cujo .pc declara
# 'Requires: mixr', e a propriedade se perderia em silencio.
#
# Estes nos sao exercitados pelo test_native, que ja linka o SDK. A arvore de
# PRODUCAO (flight_tree.xml) nao os usa, entao test_tree continua cobrindo
# tudo o que ela contem.
bt_sdk_sources = files(
    '../src/bt/bt_factory_sdk.cpp',
    '../src/bt/nodes/OnnxScoreCondition.cpp',
    '../src/bt/nodes/OnnxPolicyAction.cpp',
    '../src/bt/nodes/PyDecideAction.cpp',
)

# O XML de PRODUCAO, por caminho -- nao uma copia.
flight_tree_xml = meson.current_source_dir() / '../configs/flight_tree.xml'
flight_tree_missile_demo_xml = meson.current_source_dir() / '../configs/flight_tree_missile_demo.xml'

test_tree = executable('test-tree',
    files('tree/test_flight_tree.cpp', 'tree/test_flight_tree_missile_demo.cpp') + bt_sources + domain_sources,
    cpp_args            : ['-DFLIGHT_TREE_XML="' + flight_tree_xml + '"',
                           '-DFLIGHT_TREE_MISSILE_DEMO_XML="' + flight_tree_missile_demo_xml + '"'],
    link_args           : ['-Wl,--disable-new-dtags'],
    include_directories : [model_inc, include_directories('.')],
    dependencies        : [gtest_dep, gtest_main_dep, behavior_tree_dep],
    install             : false,
)
test('tree', test_tree, suite: 'tree', timeout: 60)

# Camada 3 -- as classes MIXR proprias, SEM levantar Station.
#
# Esta linka o MIXR (as outras duas nao) e o SDK -- e o preco de testar o que
# de fato herda do framework. Mas nao levanta simulacao: nada aqui tem
# WorldModel, Station, terreno ou JSBSim.
#
# Reusa os MESMOS fontes que vao para o .so, e nao uma copia.
native_sources = files(
    '../src/xnative/AlertDatalink.cpp',
    '../src/xnative/ThreadTag.cpp',
    '../src/xnative/RadarScan.cpp',
    '../src/xnative/FlightAgentTC.cpp',
    '../src/xnative/factory.cpp',
    '../src/ubf/FlightState.cpp',
    '../src/ubf/FlightAction.cpp',
    '../src/ubf/BtBehavior.cpp',
    '../src/ubf/BtBehaviorSlots.cpp',
    '../src/ubf/AltitudeSafetyBehavior.cpp',
    '../src/ubf/RLBridgeBehavior.cpp',
)

test_native = executable('test-native',
    files('native/test_xnative.cpp', 'native/test_rl_bridge_behavior.cpp', 'native/test_flight_state_action.cpp') + native_sources + bt_sources + bt_sdk_sources + domain_sources,
    link_args           : ['-Wl,--disable-new-dtags'],
    include_directories : model_inc,
    dependencies        : [gtest_dep, gtest_main_dep, mixr_dep, behavior_tree_dep, sdk_dep],
    install             : false,
    build_rpath         : mixr_libdir + ':' + sdk_libdir,
)
test('native', test_native, suite: 'native', timeout: 60)
