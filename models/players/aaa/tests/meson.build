# ==============================================================================
# Suite deste modelo -- tres camadas: a regra pura (domain), a arvore de
# comportamento (tree, mais a guarda de sincronia do <TreeNodesModel>) e a
# FORMA do artefato (contract). models/players/A-4 tem uma quarta, 'native',
# que levanta as classes MIXR do modelo -- fora de escopo aqui (mesmo
# raciocinio do template que originou este scaffold: AaaState/AaaBehavior/
# AaaAction dependem de um Player/WorldModel de verdade para exercitar,
# fora do alcance de um teste sem Station).
#
# 'model_inc' e 'aaa_lib' vem do escopo do meson.build PAI (Meson
# compartilha o namespace entre um project() e os subdir() dele) -- nao sao
# redeclarados aqui.
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
# Acrescente um 'test_XXX.cpp' aqui ao lado a cada regra nova em domain/.
# 'domain_sources' vem do meson.build PAI -- os MESMOS arquivos que o .so
# compila, nunca uma copia.
test_domain = executable('test-domain',
    files('domain/test_DomePolicy.cpp') + domain_sources,
    include_directories : model_inc,
    dependencies        : [gtest_dep, gtest_main_dep],
    install             : false,
)
test('domain', test_domain, suite: 'domain', timeout: 60)

# Camada 2 -- a ARVORE de producao, carregada de verdade contra um contexto
# falso. Linka BT.CPP e domain/, NAO linka MIXR: e' o que a interface
# bt/DecisionContext.hpp compra (ver o cabecalho do .cpp abaixo).
aaa_tree_xml = meson.current_source_dir() / '../configs/aaa_tree.xml'

test_tree = executable('test-tree',
    files('tree/test_aaa_tree.cpp') + bt_sources + domain_sources,
    cpp_args            : ['-DAAA_TREE_XML="' + aaa_tree_xml + '"'],
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
#
# Passa o caminho de VERDADE do binario (nao o default do script, que supoe
# um diretorio de build chamado 'build'): o Meson resolve o objeto
# 'dump_tree_model' -- vindo de ../tools/meson.build, cujo subdir() roda
# ANTES deste -- pro caminho de saida real e ja encadeia a dependencia de
# build.
py3 = find_program('python3')
test('tree-model-sync',
    py3,
    args    : [files('../tools/update_bt_models.py'), '--check', '--binary', dump_tree_model],
    suite   : 'tree',
    timeout : 60,
)

# Camada 3 -- a FORMA do artefato (nao o comportamento): exatamente um
# simbolo T exportado (o ponto de entrada de MIXR_PLUGIN_DEFINE) e nenhuma
# dependencia dinamica nao resolvida -- os dois criterios que CONTRIBUTING.md
# secao 5.1 (raiz) ja documentava a mao.
check_contract = find_program('check_contract.sh')
test('contrato-simbolo-unico-e-deps-resolvidas', check_contract,
    args    : [aaa_lib],
    suite   : 'contract',
    timeout : 30,
)
