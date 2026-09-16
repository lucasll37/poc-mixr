# ==============================================================================
# Suite deste modelo -- quatro camadas: a regra pura (domain), a arvore de
# comportamento (tree, mais a guarda de sincronia do <TreeNodesModel>), as
# classes MIXR proprias sem Station (native) e a FORMA do artefato (contract).
#
# 'model_inc' e 'navstar3_lib' vem do escopo do meson.build PAI (Meson
# compartilha o namespace entre um project() e os subdir() dele).
#
# ARMADILHA MEDIDA (documentada em CLAUDE.md, raiz, e em todo meson.build de
# modelo deste repositorio): 'meson test' devolve rc=0 para suite VAZIA
# ("No tests defined."), e o default de -Dtests e false. O Makefile confere
# a contagem com 'meson introspect --tests' antes de rodar -- ver o alvo
# 'test' dele.
# ==============================================================================
gtest_dep      = dependency('gtest', method: 'pkg-config')
gtest_main_dep = dependency('gtest_main', method: 'pkg-config')

# Camada 1 -- a regra pura: sem MIXR, sem BT.CPP, sem rpath nenhum.
# 'domain_sources' vem do meson.build PAI -- os MESMOS arquivos que o .so
# compila, nunca uma copia.
test_domain = executable('test-domain',
    files('domain/test_CircularOrbit.cpp', 'domain/test_EclipseGeometry.cpp') + domain_sources,
    include_directories : model_inc,
    dependencies        : [gtest_dep, gtest_main_dep],
    install             : false,
)
test('domain', test_domain, suite: 'domain', timeout: 60)

# Camada 2 -- a ARVORE de producao, carregada de verdade contra um contexto
# falso. Linka BT.CPP e domain/, NAO linka MIXR: e' o que a interface
# bt/DecisionContext.hpp compra.
navstar3_tree_xml = meson.current_source_dir() / '../configs/navstar3_sun_tree.xml'

test_tree = executable('test-tree',
    files('tree/test_navstar3_tree.cpp') + bt_sources + domain_sources,
    cpp_args            : ['-DNAVSTAR3_TREE_XML="' + navstar3_tree_xml + '"'],
    link_args           : rpath_link_args,
    include_directories : [model_inc, include_directories('.')],
    dependencies        : [gtest_dep, gtest_main_dep, behavior_tree_dep],
    install             : false,
)
test('tree', test_tree, suite: 'tree', timeout: 60)

# Guarda: TODA arvore .xml de configs/ (descoberta por CONTEUDO -- ver
# ../tools/update_bt_models.py, o mesmo script que 'make update-bt' roda) tem
# que ter o <TreeNodesModel> batendo com o que src/bt/bt_factory.cpp registra
# DE VERDADE. Sem isto, registrar um no novo e' um "verde silencioso" ate
# alguem tentar abrir a arvore no Groot e levar "This model has not been
# registered: <ID>".
py3 = find_program('python3')
test('tree-model-sync',
    py3,
    args    : [files('../tools/update_bt_models.py'), '--check', '--binary', dump_tree_model],
    suite   : 'tree',
    timeout : 60,
)

# Camada 3 -- as classes MIXR proprias, SEM levantar Station.
#
# Esta linka o MIXR (as outras duas nao) e o SDK -- e o preco de testar o que
# de fato herda do framework. Mas nao levanta simulacao: nada aqui tem
# WorldModel, Station, terreno ou JSBSim de verdade (so os bancos de
# bancada -- ver o Bench de tests/native/test_navstar3.cpp). Nao ha classe de
# "corpo fisico" propria neste modelo (ao contrario de paratrooper) --
# ( SpaceVehicle ) e' nativa, ver o comentario em src/xnative/factory.cpp.
#
# Reusa os MESMOS fontes que vao para o .so, e nao uma copia.
native_sources = files(
    '../src/xnative/Navstar3AgentTC.cpp',
    '../src/xnative/factory.cpp',
    '../src/ubf/Navstar3State.cpp',
    '../src/ubf/Navstar3BtBehavior.cpp',
    '../src/ubf/Navstar3Action.cpp',
)

test_native = executable('test-native',
    files('native/test_xnative.cpp', 'native/test_navstar3.cpp')
        + native_sources + bt_sources + domain_sources,
    link_args           : rpath_link_args,
    include_directories : model_inc,
    dependencies        : [gtest_dep, gtest_main_dep, mixr_dep, behavior_tree_dep, sdk_dep],
    install             : false,
    build_rpath         : mixr_libdir + ':' + sdk_libdir,
)
test('native', test_native, suite: 'native', timeout: 60)

# Camada 4 -- a FORMA do artefato (nao o comportamento): exatamente um
# simbolo T exportado (o ponto de entrada de MIXR_PLUGIN_DEFINE) e nenhuma
# dependencia dinamica nao resolvida.
check_contract = find_program('check_contract.sh')
test('contrato-simbolo-unico-e-deps-resolvidas', check_contract,
    args    : [navstar3_lib],
    suite   : 'contract',
    timeout : 30,
)
