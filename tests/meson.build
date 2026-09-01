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
#   plugin       carga dinamica de modelos (dlopen): contrato, falhas e a
#                prova de que o comportamento vem da .so e nao do executavel
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


# ------------------------------------------------------------------------------
# Camada 6 -- carga dinamica do MODELO (shared/xplugin)
#
# O modelo (domain/, bt/, ubf/, xnative/) nao esta dentro do executavel: e um
# shared_module aberto com dlopen durante o parse do cenario. Esta camada prova
# que o contrato e cumprido, que as falhas sao legiveis, e que o comportamento
# vem mesmo do .so.
#
# So a single-thread. As gemeas diferem so no agente do UBF, e toda a fiacao de
# plugin e byte-identica entre elas -- com guarda propria em
# check_duplication.sh. Rodar nas duas custaria o dobro sem cobrir nada a mais.
# ------------------------------------------------------------------------------
poc_src_dir = poc_dir / 'src'

# As duas variantes do MESMO modelo, diferindo so no sentido da curva do
# circuito de patrulha (ver o comentario em domain/PatrolPlan.cpp). E o par que
# prova a tese: mesmo executavel, .so diferente, trajetoria diferente -- sem
# recompilar nem relinkar o binario.
#
# Escolhido entre as coisas que o EDL NAO sobrescreve: se fosse um slot, o
# teste provaria so que a configuracao funciona, e nao que CODIGO novo entrou
# no processo.
model_variant_srcs = files(
    poc_src_dir / 'plugin.cpp',
    poc_src_dir / 'domain/geometry.cpp',
    poc_src_dir / 'domain/PatrolPlan.cpp',
    poc_src_dir / 'domain/RtbPlan.cpp',
    poc_src_dir / 'domain/TerrainFloor.cpp',
    poc_src_dir / 'domain/ThreatPolicy.cpp',
    poc_src_dir / 'xnative/AlertDatalink.cpp',
    poc_src_dir / 'xnative/TacticalAlert.cpp',
    poc_src_dir / 'xnative/factory.cpp',
    poc_src_dir / 'xnative/ThreadTag.cpp',
    poc_src_dir / 'xnative/TrackQuery.cpp',
    poc_src_dir / 'ubf/FlightState.cpp',
    poc_src_dir / 'ubf/FlightAction.cpp',
    poc_src_dir / 'ubf/BtBehavior.cpp',
    poc_src_dir / 'ubf/BtBehaviorSlots.cpp',
    poc_src_dir / 'ubf/AltitudeSafetyBehavior.cpp',
    poc_src_dir / 'bt/bt_factory.cpp',
    poc_src_dir / 'bt/nodes/AlertReceivedCondition.cpp',
    poc_src_dir / 'bt/nodes/ContactDetectedCondition.cpp',
    poc_src_dir / 'bt/nodes/FuelLowCondition.cpp',
    poc_src_dir / 'bt/nodes/PatrolAction.cpp',
    poc_src_dir / 'bt/nodes/ReportAndEvadeAction.cpp',
    poc_src_dir / 'bt/nodes/ReturnToBaseAction.cpp',
    poc_src_dir / 'bt/nodes/SupportAlertAction.cpp',
)

model_variants = []
foreach v : [['a', '1.0'], ['b', '-1.0']]
    model_variants += shared_module('model_variant_' + v[0],
        model_variant_srcs,
        cpp_args            : ['-DPOC_MODEL_TURN_SIGN=' + v[1]],
        link_args           : rpath_link_args + ['-Wl,--no-undefined', '-Wl,--exclude-libs,ALL'],
        include_directories : poc_inc,
        dependencies        : [thread_dep, mixr_dep, behavior_tree_dep,
                               xlog_dep, xboard_dep, xplugin_abi_dep],
        gnu_symbol_visibility : 'hidden',
        build_rpath         : mixr_libdir,
        install             : false,
    )
endforeach

# Plugins DEFEITUOSOS DE PROPOSITO. Ficam aqui, e nao junto do modelo, porque
# so existem para teste -- um build comum nao deve produzi-los.
bad_plugins = {}
foreach b : ['nosym', 'abi', 'collide']
    bad_plugins += {b: shared_module('bad_' + b,
        files('plugin/bad_' + b + '.cpp'),
        link_args           : rpath_link_args + ['-Wl,--no-undefined'],
        dependencies        : [mixr_dep, xplugin_abi_dep],
        # 'hidden' e o que faz o bad_nosym reproduzir a armadilha: sem o
        # MIXR_PLUGIN_EXPORT o ponto de entrada dele nao chega ao .dynsym.
        gnu_symbol_visibility : 'hidden',
        build_rpath         : mixr_libdir,
        install             : false,
    )}
endforeach

# 6a -- o contrato, sem levantar Station. NAO linka codigo de poc nenhuma:
# faz parte da afirmacao que o modelo nao depende da aplicacao.
test_plugin_load = executable('test-plugin-load',
    files('plugin/test_plugin_load.cpp'),
    cpp_args            : ['-DMODEL_PLUGIN_SO="' + single_thread_model.full_path() + '"'],
    link_args           : rpath_link_args,
    include_directories : include_directories('../shared'),
    dependencies        : [gtest_dep, gtest_main_dep, mixr_dep, xplugin_abi_dep],
    install             : false,
    build_rpath         : mixr_libdir,
)
test('plugin-contrato', test_plugin_load,
     depends: single_thread_model, suite: 'plugin', timeout: 60)

# 6b -- a guarda de simbolo. Com o modelo, ela passou a ter dente de verdade:
# a BehaviorTree.CPP e ESTATICA e traz 447 simbolos T globais, que so nao
# vazam por causa do -Wl,--exclude-libs,ALL no alvo do plugin.
test('plugin-simbolo', bash,
     args: files('plugin/check_plugin_symbol.sh') + [single_thread_model.full_path()],
     depends: single_thread_model,
     workdir: meson.project_source_root(),
     suite: 'plugin', timeout: 60)

# 6c -- os modos de falha. Cada caso afirma rc != 139: sem a checagem de nome
# desconhecido em mixrFactory(), dois deles voltariam a ser SIGSEGV mudo.
test('plugin-negativos', py,
     args: files('plugin/run_plugin_negatives.py') + [
         '--binario', single_thread.full_path(),
         '--poc', 'single-thread',
         '--bad-nosym', bad_plugins['nosym'].full_path(),
         '--bad-abi', bad_plugins['abi'].full_path(),
         '--bad-collide', bad_plugins['collide'].full_path()],
     depends: [single_thread, single_thread_model,
               bad_plugins['nosym'], bad_plugins['abi'], bad_plugins['collide']],
     workdir: meson.project_source_root(),
     suite: 'plugin', timeout: 900, is_parallel: false)

# 6d -- a tese: mesmo binario, modelo diferente, voo diferente
test('plugin-hotswap', py,
     args: files('plugin/run_plugin_hotswap.py') + [
         '--binario', single_thread.full_path(),
         '--poc', 'single-thread',
         '--so-a', model_variants[0].full_path(),
         '--so-b', model_variants[1].full_path()],
     depends: [single_thread] + model_variants,
     workdir: meson.project_source_root(),
     suite: 'plugin', timeout: 900, is_parallel: false)

# Guarda -- invariantes estruturais do repositorio
test('duplicacao-domain-bt',
     bash,
     args: files('guard/check_duplication.sh'),
     workdir: meson.project_source_root(),
     suite: 'guard',
     timeout: 60)
