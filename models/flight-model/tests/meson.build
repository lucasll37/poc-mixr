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
    '../src/domain/PatrolPlan.cpp',
    '../src/domain/RtbPlan.cpp',
    '../src/domain/TerrainFloor.cpp',
    '../src/domain/ThreatPolicy.cpp',
)

# Camada 1 -- regras puras: sem MIXR, sem BehaviorTree.CPP, sem rpath nenhum.
test_domain = executable('test-domain',
    files(
        'domain/test_geometry.cpp',
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
    '../src/bt/nodes/PatrolAction.cpp',
    '../src/bt/nodes/ReportAndEvadeAction.cpp',
    '../src/bt/nodes/ReturnToBaseAction.cpp',
    '../src/bt/nodes/SupportAlertAction.cpp',
)

# O XML de PRODUCAO, por caminho -- nao uma copia.
flight_tree_xml = meson.current_source_dir() / '../configs/flight_tree.xml'

test_tree = executable('test-tree',
    files('tree/test_flight_tree.cpp') + bt_sources + domain_sources,
    cpp_args            : ['-DFLIGHT_TREE_XML="' + flight_tree_xml + '"'],
    link_args           : ['-Wl,--disable-new-dtags'],
    include_directories : [model_inc, include_directories('.')],
    dependencies        : [gtest_dep, gtest_main_dep, behavior_tree_dep],
    install             : false,
)
test('tree', test_tree, suite: 'tree', timeout: 60)
