# ==============================================================================
# Suite de testes -- so entra no build com -Dtests=true (ver meson_options.txt)
#
# Cinco camadas, da mais rapida e isolada para a mais cara e integrada:
#
#   domain       regras puras, sem MIXR e sem BehaviorTree.CPP
#   tree         a arvore de producao contra um contexto falso, sem Station
#   scenario     o binario de verdade, com cenario de teste, por -f
#   memory       vazamento, lido do metadado de instancias do proprio MIXR
#   determinism  o mesmo estado com 1, 2 e 4 threads, nos DOIS lacos de decisao
#   guard        invariantes estruturais do repositorio
#
# As duas primeiras camadas nao linkam MIXR: nao precisam de rpath nenhum.
# ==============================================================================

gtest_dep = dependency('gtest', method: 'pkg-config')
gtest_main_dep = dependency('gtest_main', method: 'pkg-config')

# As pocs sao gemeas; domain/ e bt/ sao byte-identicos entre elas (ha um teste
# de guarda para isso). Os testes compilam contra a copia da single-thread.
poc_dir = meson.project_source_root() / 'src' / 'single-thread'
poc_inc = include_directories('../src/single-thread/include')

# ------------------------------------------------------------------------------
# Camada 1 -- domain/ puro
# ------------------------------------------------------------------------------
domain_sources = files(
    poc_dir / 'src/domain/geometry.cpp',
    poc_dir / 'src/domain/PatrolPlan.cpp',
    poc_dir / 'src/domain/RtbPlan.cpp',
    poc_dir / 'src/domain/TerrainFloor.cpp',
    poc_dir / 'src/domain/ThreatPolicy.cpp',
)

# As primitivas de deteccao do shared/xmsg (limiar com histerese, deadband com
# memoria, derivada em janela, piso de emissao). Moram em shared/ e nao numa
# poc, mas sao tao livres de MIXR quanto domain/ -- entram no mesmo binario
# pelo mesmo motivo: sao regra pura e rodam em milissegundos.
xmsg_rules_dir = meson.project_source_root() / 'shared' / 'xmsg'
xmsg_rules_inc = include_directories('../shared')
xmsg_rules_sources = files(
    xmsg_rules_dir / 'rules/Deadband.cpp',
    xmsg_rules_dir / 'rules/EmitGate.cpp',
    xmsg_rules_dir / 'rules/RateWindow.cpp',
    xmsg_rules_dir / 'rules/Schmitt.cpp',
)

test_domain = executable('test-domain',
    files(
        'domain/test_geometry.cpp',
        'domain/test_TerrainFloor.cpp',
        'domain/test_ThreatPolicy.cpp',
        'domain/test_PatrolPlan.cpp',
        'domain/test_RtbPlan.cpp',
        'domain/test_xmsg_rules.cpp',
    ) + domain_sources + xmsg_rules_sources,
    include_directories : [poc_inc, xmsg_rules_inc],
    dependencies        : [gtest_dep, gtest_main_dep],
    install             : false,
)
test('domain', test_domain, suite: 'domain', timeout: 60)

# ------------------------------------------------------------------------------
# Camada 2 -- a arvore de comportamento
#
# Linka domain/ + bt/ + BehaviorTree.CPP. NAO linka MIXR: e exatamente isso
# que o refactor de bt/DecisionContext.hpp comprou. O rpath do BT.CPP e
# necessario porque essa lib vive no cache do Conan.
# ------------------------------------------------------------------------------
bt_sources = files(
    poc_dir / 'src/bt/bt_factory.cpp',
    poc_dir / 'src/bt/nodes/AlertReceivedCondition.cpp',
    poc_dir / 'src/bt/nodes/ContactDetectedCondition.cpp',
    poc_dir / 'src/bt/nodes/FuelLowCondition.cpp',
    poc_dir / 'src/bt/nodes/PatrolAction.cpp',
    poc_dir / 'src/bt/nodes/ReportAndEvadeAction.cpp',
    poc_dir / 'src/bt/nodes/ReturnToBaseAction.cpp',
    poc_dir / 'src/bt/nodes/SupportAlertAction.cpp',
)

# O XML de PRODUCAO, por caminho -- nao uma copia (ver o cabecalho do teste).
flight_tree_xml = poc_dir / 'configs/flight_tree.xml'

test_tree = executable('test-tree',
    files('tree/test_flight_tree.cpp') + bt_sources + domain_sources,
    cpp_args            : ['-DFLIGHT_TREE_XML="' + flight_tree_xml + '"'],
    link_args           : rpath_link_args,
    include_directories : [poc_inc, include_directories('.')],
    dependencies        : [gtest_dep, gtest_main_dep, behavior_tree_dep],
    install             : false,
    build_rpath         : bt_libdir,
)
test('tree', test_tree, suite: 'tree', timeout: 60)

# ------------------------------------------------------------------------------
# Camadas 3, 4 e 5 + guarda -- testes de script sobre os binarios das pocs
#
# is_parallel: false em tudo que roda um executavel da poc. Nao e cautela
# generica: app/ScenarioTemplate grava o cenario expandido sempre no MESMO
# caminho (src/<poc>/configs/scenario.generated.epp, nao configuravel por
# linha de comando), entao duas execucoes simultaneas disputam o arquivo.
#
# Todos usam fixtures HERMETICAS (sem 'networks:'). Com o bloco de rede o
# processo abre a porta DIS 3000 e ingere PDUs de quem estiver na LAN --
# medido: uma sessao de bandit-dis no ar muda o resultado de duas execucoes
# identicas. Ver tests/scenario/make_fixture.py.
# ------------------------------------------------------------------------------
py = find_program('python3')
bash = find_program('bash')

pocs = [
    # nome            binario         onde a decisao roda
    ['single-thread', single_thread,  'nao-critico'],
    ['multi-thread',  multi_thread,   'critico'],
]

scenario_runner = files('scenario/run_scenario_test.py')
leak_runner     = files('memory/run_leak_test.py')
determinism_sh  = files('determinism/check_determinism.sh')

foreach poc : pocs
    nome = poc[0]
    bin  = poc[1]
    laco = poc[2]

    # Camada 3 -- cenario ponta a ponta, um caso por ramo da arvore
    foreach modo : ['intruder', 'lowfuel', 'terrain']
        test('scenario-' + modo + '-' + nome,
             py,
             args: scenario_runner + ['--binario', bin.full_path(),
                                      '--poc', nome, '--mode', modo,
                                      '--frames', '800'],
             depends: bin,
             workdir: meson.project_source_root(),
             suite: 'scenario',
             timeout: 900,
             is_parallel: false)
    endforeach

    # Camada 4 -- vazamento pelo metadado de instancias do MIXR
    test('memory-' + nome,
         py,
         args: leak_runner + [bin.full_path(), nome],
         depends: bin,
         workdir: meson.project_source_root(),
         suite: 'memory',
         timeout: 1800,
         is_parallel: false)

    # Camada 5 -- determinismo, nomeado por ONDE a decisao roda
    test('determinism-' + laco,
         bash,
         args: determinism_sh + [bin.full_path(), nome, '600', nome],
         depends: bin,
         workdir: meson.project_source_root(),
         suite: 'determinism',
         timeout: 1800,
         is_parallel: false)
endforeach

# Guarda -- invariantes estruturais do repositorio
test('duplicacao-domain-bt',
     bash,
     args: files('guard/check_duplication.sh'),
     workdir: meson.project_source_root(),
     suite: 'guard',
     timeout: 60)
