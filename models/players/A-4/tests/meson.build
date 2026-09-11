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

# domain_sources/bt_sources/bt_sdk_sources vem do meson.build do projeto (um
# nivel acima) -- compartilhados com tools/dump-tree-model, que reusa os
# MESMOS fontes.

# O SDK SO PELOS HEADERS -- sem uma unica biblioteca no link.
#
# domain/ inclui "xrandom/DeterministicRng.hpp" (o gerador de toda a
# aleatoriedade deste repositorio mora la, ver libs/xrandom). Esse header e
# header-only e nao depende de nada -- mas so fica visivel via dist/include,
# que quem publica e o sdk_dep. Usar o sdk_dep INTEIRO nos dois alvos abaixo
# arrastaria 'Requires: mixr' do .pc para o link e destruiria em silencio a
# propriedade que test_tree existe para ter ("NAO linka o MIXR").
#
# partial_dependency(includes/compile_args) da os -I e para ai. Conferido por
# 'ldd' nos dois binarios: zero libs do MIXR.
sdk_headers_dep = sdk_dep.partial_dependency(includes: true, compile_args: true)

# Camada 1 -- regras puras: sem MIXR, sem BehaviorTree.CPP, sem rpath nenhum.
test_domain = executable('test-domain',
    files(
        'domain/test_geometry.cpp',
        'domain/test_TerrainFloor.cpp',
        'domain/test_ThreatPolicy.cpp',
        'domain/test_LaunchPolicy.cpp',
        'domain/test_PatrolPlan.cpp',
        'domain/test_RtbPlan.cpp',
        'domain/test_AerobaticPlan.cpp',
        'domain/test_EvasionReactionPlan.cpp',
    ) + domain_sources,
    include_directories : model_inc,
    dependencies        : [gtest_dep, gtest_main_dep, sdk_headers_dep],
    install             : false,
)
test('domain', test_domain, suite: 'domain', timeout: 60)

# Camada 2 -- a arvore de producao contra um contexto falso.
# Linka BehaviorTree.CPP; NAO linka MIXR -- e o que o refactor de
# bt/DecisionContext.hpp comprou, e continua valendo aqui.
#
# bt_sources (herdado do meson.build do projeto) e compartilhado por
# test_tree e test_native, e test_tree tem uma propriedade que vale a pena
# preservar: ele NAO linka o MIXR (ver o comentario do alvo, logo abaixo) --
# e o que o refactor de bt/DecisionContext.hpp comprou. Um no que chama
# libs/xinfer arrasta sdk_dep, cujo .pc declara 'Requires: mixr', e a
# propriedade se perderia em silencio -- por isso os nos que dependem do SDK
# ficam em bt_sdk_sources (tambem herdado), FORA de bt_sources.
#
# Estes nos sao exercitados pelo test_native, que ja linka o SDK --
# native/test_onnx_nodes.cpp cobre OnnxScoreCondition/OnnxPolicyAction
# diretamente (porta 'model' ausente, caminho inexistente, indice fora da
# faixa; achado por auditoria: ate essa suite existir, a cobertura real era
# so' scenario-policy-onnx, o binario completo, que prova o caminho feliz
# mas nao os casos de borda). A arvore de PRODUCAO (flight_tree.xml) nao os
# usa, entao test_tree continua cobrindo
# tudo o que ela contem.

# O XML de PRODUCAO, por caminho -- nao uma copia.
flight_tree_xml = meson.current_source_dir() / '../configs/flight_tree.xml'
flight_tree_nav_xml = meson.current_source_dir() / '../configs/flight_tree_nav.xml'
flight_tree_random_xml = meson.current_source_dir() / '../configs/flight_tree_random.xml'
flight_tree_missile_demo_xml = meson.current_source_dir() / '../configs/flight_tree_missile_demo.xml'
flight_tree_rwr_evade_demo_xml = meson.current_source_dir() / '../configs/flight_tree_rwr_evade_demo.xml'

# O MESMO .onnx de pesos aleatorios que scenario-policy-onnx ja usa (forma
# certa -- 28 entradas -> 3 saidas -- pesos irrelevantes pros testes de
# test_onnx_nodes.cpp, que so afirmam sobre forma/portas/rotulo).
policy_example_onnx = meson.current_source_dir() / '../configs/policy_example.onnx'

# 'dump_tree_model' vem de tools/meson.build (subdir() chamado ANTES deste,
# incondicional, no meson.build do projeto) -- o executavel em si nao e
# teste, so o 'tree-model-sync' abaixo e.

# Guarda: TODA arvore .xml de configs/ (descoberta por conteudo -- ver
# tools/update_bt_models.py, o mesmo script de 'make update-bt') tem que ter
# a tag <TreeNodesModel> batendo com o que bt_factory.cpp/bt_factory_sdk.cpp
# registram DE VERDADE -- sem isto, um no novo registrado e' um "verde
# silencioso" ate alguem tentar abrir a arvore no Groot e levar "This model
# has not been registered". Passa o caminho de VERDADE do binario (nao o
# default de update_bt_models.py, que supoe um diretorio chamado 'build') --
# Meson resolve o objeto dump_tree_model pro caminho de saida e ja adiciona
# a dependencia de build.
py3 = find_program('python3')
test('tree-model-sync',
    py3,
    args      : [files('../tools/update_bt_models.py'), '--check', '--binary', dump_tree_model],
    suite     : 'tree',
    timeout   : 60,
)

test_tree = executable('test-tree',
    files('tree/test_flight_tree.cpp',
          'tree/test_flight_tree_nav.cpp',
          'tree/test_flight_tree_random.cpp',
          'tree/test_flight_tree_missile.cpp',
          'tree/test_flight_tree_rwr_evade_demo.cpp') + bt_sources + domain_sources,
    cpp_args            : ['-DFLIGHT_TREE_XML="' + flight_tree_xml + '"',
                           '-DFLIGHT_TREE_NAV_XML="' + flight_tree_nav_xml + '"',
                           '-DFLIGHT_TREE_RANDOM_XML="' + flight_tree_random_xml + '"',
                           '-DFLIGHT_TREE_MISSILE_DEMO_XML="' + flight_tree_missile_demo_xml + '"',
                           '-DFLIGHT_TREE_RWR_EVADE_DEMO_XML="' + flight_tree_rwr_evade_demo_xml + '"'],
    link_args           : ['-Wl,--disable-new-dtags'],
    include_directories : [model_inc, include_directories('.')],
    dependencies        : [gtest_dep, gtest_main_dep, behavior_tree_dep, sdk_headers_dep],
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
    '../src/xnative/ThreadTagProbe.cpp',
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
    files('native/test_xnative.cpp', 'native/test_rl_bridge_behavior.cpp',
          'native/test_flight_state_action.cpp', 'native/test_onnx_nodes.cpp')
        + native_sources + bt_sources + bt_sdk_sources + domain_sources,
    cpp_args            : ['-DPOLICY_EXAMPLE_ONNX="' + policy_example_onnx + '"'],
    link_args           : ['-Wl,--disable-new-dtags'],
    include_directories : model_inc,
    dependencies        : [gtest_dep, gtest_main_dep, mixr_dep, behavior_tree_dep, sdk_dep],
    install             : false,
    build_rpath         : mixr_libdir + ':' + sdk_libdir,
)
test('native', test_native, suite: 'native', timeout: 60)
