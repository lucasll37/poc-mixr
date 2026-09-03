# ==============================================================================
# Suite do HOST -- so entra no build com -Dtests=true (ver meson_options.txt)
#
# As camadas 'domain' e 'tree' NAO estao mais aqui: elas compilam domain/ e bt/
# direto, e esses fontes sairam para models/flight/. Moraram junto com o que
# testam -- rode 'make test-models' (ou 'make test', que roda as duas).
#
#   domain-rules  as primitivas de deteccao do shared/xmsg (host, ficaram)
#   scenario      o binario de verdade, com fixture, asserções sobre 'frame='
#   memory        vazamento, pelos contadores de instancia do MIXR
#   determinism   o mesmo estado com 1, 2 e 4 threads
#   plugin        o contrato de carga dinamica e os modos de falha
#   guard         invariantes estruturais -- inclusive a OPACIDADE do host
#
# MUDANCA IMPORTANTE: o plugin de modelo nao e mais um alvo deste projeto. Ele
# vem pronto de dist/lib/mixr-plugins/, construido antes por 'make models'.
# Isso significa que o meson NAO tem aresta de dependencia ate ele -- por isso
# existe a guarda de FRESCOR (check_modelo_fresco.sh).
# ==============================================================================

gtest_dep = dependency('gtest', method: 'pkg-config')
gtest_main_dep = dependency('gtest_main', method: 'pkg-config')

py = find_program('python3')
bash = find_program('bash')

raiz = meson.project_source_root()
dist_plugins = raiz / 'dist' / 'lib' / 'mixr-plugins'
# 'models/flight' e um projeto Meson AUTOCONTIDO (ver models/README.md
# Sec1.1) -- constroi em './build' DENTRO da propria pasta, nao mais em
# 'build-flight' na raiz (esse era o caminho do design anterior, quando
# 'make models' fazia o setup do flight direto por aqui). As variantes A/B
# de teste ('libmodel_variant_a.so'/'_b.so', para plugin-hotswap) saem la.
model_build  = raiz / 'models' / 'flight' / 'build'

# ------------------------------------------------------------------------------
# Camada 1 -- as primitivas de deteccao do shared/xmsg
#
# Sao do HOST (shared/xmsg), nao do modelo, entao ficaram. Tao livres de MIXR
# quanto o domain/ que foi embora: regra pura, milissegundos.
# ------------------------------------------------------------------------------
xmsg_rules_dir = raiz / 'shared' / 'xmsg'
test_domain_rules = executable('test-domain-rules',
    files('domain/test_xmsg_rules.cpp') + files(
        xmsg_rules_dir / 'rules/Deadband.cpp',
        xmsg_rules_dir / 'rules/EmitGate.cpp',
        xmsg_rules_dir / 'rules/RateWindow.cpp',
        xmsg_rules_dir / 'rules/Schmitt.cpp',
    ),
    include_directories : include_directories('../shared'),
    dependencies        : [gtest_dep, gtest_main_dep],
    install             : false,
)
test('domain-rules', test_domain_rules, suite: 'domain', timeout: 60)

# A logica pura (sem MIXR, sem FTXUI) por tras da aba "Memoria" do dashboard
# (./app/, fora de src/ -- ver o cabecalho de ../app/include/app/
# MetaObjectSnapshot.hpp). Mesmo motivo de estar na suite 'domain': regra
# isolada, milissegundos.
test_app_meta = executable('test-app-meta',
    files('app/test_meta_object_snapshot.cpp') + files(
        '../app/src/app/MetaObjectSnapshot.cpp',
    ),
    link_args           : rpath_link_args,
    include_directories : include_directories('../app/include'),
    dependencies        : [gtest_dep, gtest_main_dep, mixr_dep, xplugin_dep],
    install             : false,
    build_rpath         : mixr_libdir,
)
test('app-meta-object-snapshot', test_app_meta, suite: 'domain', timeout: 60)

# A geometria pura da aba Mapa (projecao mundo->tela e sua inversa, escolha
# de intervalo de curva de nivel) -- promovida de um namespace anonimo
# dentro de app/MapPanel.cpp para app/MapGeometry.hpp/.cpp. Ver o "porque"
# no cabecalho daquele header.
test_app_map_geometry = executable('test-app-map-geometry',
    files('app/test_map_geometry.cpp') + files(
        '../app/src/app/MapGeometry.cpp',
    ),
    link_args           : rpath_link_args,
    include_directories : include_directories('../app/include'),
    dependencies        : [gtest_dep, gtest_main_dep, mixr_dep, xplugin_dep, ftxui_dep],
    install             : false,
    build_rpath         : mixr_libdir,
)
test('app-map-geometry', test_app_map_geometry, suite: 'domain', timeout: 60)

# O canvas da aba Mapa acompanha a caixa que o LAYOUT reservou pra ele (era
# fixo em 120x30 celulas: num terminal maior sobrava quadro vazio, num
# menor o desenho saia cortado). Alem da conversao em si, o teste monta a
# MESMA composicao de app/DashboardLoop.cpp e renderiza num ftxui::Screen
# de tamanho fixo -- por isso linka MapPanel.cpp/FleetPanel.cpp/
# BehaviorTreeView.cpp, e nao so a geometria pura. Sem Station, sem
# terminal de verdade.
test_app_map_fit = executable('test-app-map-canvas-fit',
    files('app/test_map_canvas_fit.cpp') + files(
        '../app/src/app/MapPanel.cpp',
        '../app/src/app/MapGeometry.cpp',
        '../app/src/app/FleetPanel.cpp',
        '../app/src/app/BehaviorTreeView.cpp',
    ),
    link_args           : rpath_link_args,
    include_directories : include_directories('../app/include'),
    dependencies        : [gtest_dep, gtest_main_dep, mixr_dep, xplugin_dep, ftxui_dep],
    install             : false,
    build_rpath         : mixr_libdir,
)
test('app-map-canvas-fit', test_app_map_fit, suite: 'domain', timeout: 60)

# O buffer em memoria de shared/xlog -- a fonte da aba "Log" do ./app -- e
# as duas regras puras do painel (filtro por nivel minimo e o ciclo dele).
# O buffer e estado GLOBAL do processo de proposito: e o que faz o LOG(...)
# do plugin do modelo (um .so aberto por dlopen) cair no MESMO lugar que o
# do host, sem nenhuma ponte extra.
test_app_log = executable('test-app-log',
    files('app/test_log_panel.cpp') + files(
        '../app/src/app/LogPanel.cpp',
    ),
    link_args           : rpath_link_args,
    include_directories : include_directories('../app/include'),
    dependencies        : [gtest_dep, gtest_main_dep, xlog_dep, ftxui_dep],
    install             : false,
    build_rpath         : mixr_libdir,
)
test('app-log', test_app_log, suite: 'domain', timeout: 60)

# A maquina de estados do breakpoint de arvore de comportamento --
# extraida de app/DashboardLoop.cpp para app/BreakpointController.hpp/.cpp.
test_app_breakpoint = executable('test-app-breakpoint-controller',
    files('app/test_breakpoint_controller.cpp') + files(
        '../app/src/app/BreakpointController.cpp',
    ),
    link_args           : rpath_link_args,
    include_directories : include_directories('../app/include'),
    dependencies        : [gtest_dep, gtest_main_dep],
    install             : false,
    build_rpath         : mixr_libdir,
)
test('app-breakpoint-controller', test_app_breakpoint, suite: 'domain', timeout: 60)

# A escada de velocidade e a decisao de rotulo/cor do cabecalho -- extraida
# de app/DashboardLoop.cpp para app/SpeedLadder.hpp/.cpp.
test_app_speed_ladder = executable('test-app-speed-ladder',
    files('app/test_speed_ladder.cpp') + files(
        '../app/src/app/SpeedLadder.cpp',
    ),
    link_args           : rpath_link_args,
    include_directories : include_directories('../app/include'),
    dependencies        : [gtest_dep, gtest_main_dep],
    install             : false,
    build_rpath         : mixr_libdir,
)
test('app-speed-ladder', test_app_speed_ladder, suite: 'domain', timeout: 60)

# A regra de selecao de pista (filtro por lado + desempate deterministico)
# de shared/xtrack, separada da travessia AirVehicle->OnboardComputer->
# TrackManager->Track que a alimenta -- ver o "porque" em
# shared/xtrack/TrackQuery.hpp. Alimenta models/flight/src/ubf/
# FlightState.cpp (a percepcao do modelo).
test_track_selection = executable('test-track-selection',
    files('domain/test_track_selection.cpp'),
    link_args           : rpath_link_args,
    dependencies        : [gtest_dep, gtest_main_dep, xtrack_dep],
    install             : false,
    build_rpath         : mixr_libdir,
)
test('track-selection', test_track_selection, suite: 'domain', timeout: 60)

# A UNICA responsabilidade de shared/xboard -- ser um mapa seguro entre N
# escritores concorrentes (o pool T/C, um por player) e 1 leitor (o laco
# de background) -- ver o "porque" de ser a UNICA shared_library() de
# shared/ em xboard/Board.hpp. Nunca tinha teste direto antes desta
# suite: so validacao indireta pelos dumps end-to-end.
test_xboard_concurrency = executable('test-xboard-concurrency',
    files('domain/test_xboard_concurrency.cpp'),
    link_args           : rpath_link_args,
    dependencies        : [gtest_dep, gtest_main_dep, xboard_dep, thread_dep],
    install             : false,
    build_rpath         : mixr_libdir,
)
test('xboard-concurrency', test_xboard_concurrency, suite: 'domain', timeout: 60)

# shared/xinfer -- a DEGRADACAO do motor de inferencia: modelo ausente,
# arquivo que nao e .onnx, id invalido. Nenhuma dessas entradas pode derrubar
# a simulacao no meio do frame; todas tem de devolver 0/negativo e deixar o
# consumidor cair no ramo seguinte da arvore. O caminho FELIZ (inferencia de
# verdade + determinismo com 1/2/4 threads) precisa de um .onnx e vem depois.
# O .onnx DE VERDADE que models/flight instala -- por caminho, nao uma copia.
# Mesmo padrao de FLIGHT_TREE_XML em models/flight/tests/meson.build: um teste
# contra uma copia provaria que a copia esta certa, o que nao interessa.
policy_onnx = raiz / 'models' / 'flight' / 'configs' / 'policy_example.onnx'

test_xinfer = executable('test-xinfer',
    files('domain/test_xinfer.cpp'),
    cpp_args            : ['-DPOLICY_ONNX="' + policy_onnx + '"'],
    link_args           : rpath_link_args,
    dependencies        : [gtest_dep, gtest_main_dep, xinfer_dep, xrlbridge_dep, thread_dep],
    install             : false,
    build_rpath         : mixr_libdir,
)
test('xinfer-degradacao', test_xinfer, suite: 'domain', timeout: 120)

# shared/xpyembed -- degradacao E isolamento por chave. O segundo e o que
# mantem o check-multi-thread verde: a ordem de aquisicao do GIL nao e
# deterministica, entao dois avioes rodando o mesmo script NAO podem
# compartilhar globais.
test_xpyembed = executable('test-xpyembed',
    files('domain/test_xpyembed.cpp'),
    link_args           : rpath_link_args,
    dependencies        : [gtest_dep, gtest_main_dep, xpyembed_dep, thread_dep],
    install             : false,
    build_rpath         : mixr_libdir,
)
test('xpyembed-isolamento', test_xpyembed, suite: 'domain', timeout: 120)

# ------------------------------------------------------------------------------
# Camadas de cenario, memoria e determinismo
#
# is_parallel: false em tudo que roda um executavel da poc: app/ScenarioTemplate
# grava o cenario expandido sempre no MESMO caminho.
# ------------------------------------------------------------------------------
pocs = [
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

    foreach modo : ['intruder', 'lowfuel', 'terrain']
        test('scenario-' + modo + '-' + nome, py,
             args: scenario_runner + ['--binario', bin.full_path(), '--poc', nome,
                                      '--mode', modo, '--frames', '800'],
             depends: bin, workdir: raiz, suite: 'scenario',
             timeout: 900, is_parallel: false)
    endforeach

    test('memory-' + nome, py,
         args: leak_runner + [bin.full_path(), nome],
         depends: bin, workdir: raiz, suite: 'memory',
         timeout: 1800, is_parallel: false)

    test('determinism-' + laco, bash,
         args: determinism_sh + [bin.full_path(), nome, '600', nome],
         depends: bin, workdir: raiz, suite: 'determinism',
         timeout: 1800, is_parallel: false)
endforeach

# ------------------------------------------------------------------------------
# app (o dashboard, ./app/ na raiz) -- os TRES cenarios PROPRIOS (ver
# ../app/include/app/ScenarioCatalog.hpp), rodados com '-deterministic' (sem
# TUI, sem TTY nenhum). Nao usa scenario_runner (que e '-f'/fixture-based,
# especifico de single-thread/multi-thread) nem checa o canal de mensagens
# (app nao linka shared/xmsg) -- ver o cabecalho de run_app_test.py.
# ------------------------------------------------------------------------------
app_runner = files('scenario/run_app_test.py')
foreach chave : ['patrol', 'intercept', 'intercept_missile']
    test('scenario-app-' + chave, py,
         args: app_runner + ['--binario', app.full_path(),
                             '--scenario', chave, '--frames', '300'],
         depends: app, workdir: raiz, suite: 'scenario',
         timeout: 300, is_parallel: false)
endforeach

# A IDENTIDADE dos objetos no stream ACMI: tipo, cor e modelo. Nenhum dos
# testes acima olha o .acmi -- eles afirmam sobre as linhas 'frame=' do dump.
# Este abre a gravacao do cenario com missil e exige taxonomia e cor validas,
# inclusive no objeto criado em RUNTIME (o missil, cujo nome sintetico
# "W%05d" nao esta em mapa nenhum do EDL e so resolve pela identidade que o
# host publica). Ver o cabecalho do script.
test('scenario-app-tacview-identidade', py,
     args: files('scenario/run_tacview_identity_test.py') + [
         '--binario', app.full_path(), '--frames', '1200'],
     depends: app, workdir: raiz, suite: 'scenario',
     timeout: 300, is_parallel: false)

# A SAIDA do app, no caminho INTERATIVO -- o unico que os testes acima nao
# alcancam: eles rodam '-deterministic', que nao tem TUI nem thread de tempo
# critico nativa. Este sobe o binario num pty de verdade, aperta [q] e afirma
# que o processo TERMINA. Inclui o caso que de fato travava: um cliente na
# porta do Tacview que conecta e para de ler. Ver o cabecalho do script.
test('scenario-app-quit', py,
     args: files('scenario/run_app_quit_test.py') + [
         '--binario', app.full_path(), '--scenario', 'patrol'],
     depends: app, workdir: raiz, suite: 'scenario',
     timeout: 300, is_parallel: false)

# O CONTROLE NEGATIVO do determinismo: prova que ele vem de ONDE a decisao roda
# (a fase 3 do frame), e nao do passo fixo. Solta o laco de background numa
# thread propria em AMBAS as pocs e exige que a single-thread DIVIRJA e a
# ------------------------------------------------------------------------------
# A politica .onnx rodando DENTRO do frame -- ver o cabecalho do script.
#
# is_parallel: false porque roda o binario da poc quatro vezes (JSBSim
# incluso), como as demais suites de cenario.
# ------------------------------------------------------------------------------
foreach caso : [['onnx', 'flight_tree_onnx.xml', 'ONNX'],
                 ['python', 'flight_tree_py.xml', 'PY']]
    test('scenario-policy-' + caso[0], py,
         args: files('scenario/run_policy_test.py') + [
             '--binario', multi_thread.full_path(),
             '--tree', caso[1], '--label', caso[2], '--frames', '600'],
         depends: multi_thread, workdir: raiz, suite: 'scenario',
         timeout: 900, is_parallel: false)
endforeach

# multi-thread NAO. Ver o cabecalho do script.
test('onde-a-decisao-roda', py,
     args: files('determinism/check_onde_a_decisao_roda.py') + [
         '--single', single_thread.full_path(),
         '--multi', multi_thread.full_path()],
     depends: [single_thread, multi_thread],
     workdir: raiz, suite: 'determinism',
     timeout: 1800, is_parallel: false)

# ------------------------------------------------------------------------------
# Camada de plugin -- o contrato de carga dinamica
#
# O .so vem de dist/, construido pelo projeto do modelo. Os plugins
# DEFEITUOSOS continuam sendo alvos daqui: sao artefatos de teste, nao modelo.
# ------------------------------------------------------------------------------
bad_plugins = {}
foreach b : ['nosym', 'abi', 'collide']
    bad_plugins += {b: shared_module('bad_' + b,
        files('plugin/bad_' + b + '.cpp'),
        link_args           : rpath_link_args + ['-Wl,--no-undefined'],
        dependencies        : [mixr_dep, xplugin_abi_dep],
        gnu_symbol_visibility : 'hidden',
        build_rpath         : mixr_libdir,
        install             : false,
    )}
endforeach

modelo_so = dist_plugins / 'libflight.so'

test_plugin_load = executable('test-plugin-load',
    files('plugin/test_plugin_load.cpp'),
    cpp_args            : ['-DMODEL_PLUGIN_SO="' + modelo_so + '"'],
    link_args           : rpath_link_args,
    include_directories : include_directories('../shared'),
    dependencies        : [gtest_dep, gtest_main_dep, mixr_dep, xplugin_abi_dep],
    install             : false,
    build_rpath         : mixr_libdir,
)
test('plugin-contrato', test_plugin_load, suite: 'plugin', timeout: 60)

test('plugin-simbolo', bash,
     args: files('plugin/check_plugin_symbol.sh') + [modelo_so],
     workdir: raiz, suite: 'plugin', timeout: 60)

test('plugin-negativos', py,
     args: files('plugin/run_plugin_negatives.py') + [
         '--binario', single_thread.full_path(), '--poc', 'single-thread',
         '--bad-nosym', bad_plugins['nosym'].full_path(),
         '--bad-abi', bad_plugins['abi'].full_path(),
         '--bad-collide', bad_plugins['collide'].full_path()],
     depends: [single_thread, bad_plugins['nosym'], bad_plugins['abi'], bad_plugins['collide']],
     workdir: raiz, suite: 'plugin', timeout: 900, is_parallel: false)

test('plugin-hotswap', py,
     args: files('plugin/run_plugin_hotswap.py') + [
         '--binario', single_thread.full_path(), '--poc', 'single-thread',
         '--so-a', model_build / 'libmodel_variant_a.so',
         '--so-b', model_build / 'libmodel_variant_b.so'],
     depends: single_thread, workdir: raiz, suite: 'plugin',
     timeout: 900, is_parallel: false)

# O MODELO DESCONHECIDO -- a prova que o resto nao da.
#
# Roda o cenario de PRODUCAO trocando so o 'file:' do ( PluginModule ) por um
# modelo escrito SO contra o SDK publicado, sem olhar domain/, bt/ ou ubf/.
# E o unico teste que pode falhar por "o contrato nao basta".
test('plugin-modelo-estranho', py,
     args: files('plugin/run_stub_model.py') + [
         '--binario', single_thread.full_path(), '--poc', 'single-thread',
         '--stub', dist_plugins / 'libstub.so'],
     depends: single_thread, workdir: raiz, suite: 'plugin',
     timeout: 900, is_parallel: false)

# O DEPOSITO DE TERCEIRO -- prova que models/plugins/ (o deposito pra um .so
# ja compilado fora deste repositorio, ver models/plugins/README.md) nao e
# so uma pasta com um README: um .so colocado ali e de fato copiado pra
# dist/lib/mixr-plugins/ (o mesmo passo que o alvo 'models' do Makefile
# raiz faz) e o resultado E USAVEL numa simulacao de verdade -- reusa o
# stub como o "terceiro" e a MESMA bateria de plugin-modelo-estranho.
test('plugin-deposito-terceiro', py,
     args: files('plugin/run_thirdparty_deposit.py') + [
         '--binario', single_thread.full_path(), '--poc', 'single-thread',
         '--source-so', dist_plugins / 'libstub.so',
         '--run-stub-model', raiz / 'tests' / 'plugin' / 'run_stub_model.py'],
     depends: single_thread, workdir: raiz, suite: 'plugin',
     timeout: 900, is_parallel: false)

# ------------------------------------------------------------------------------
# Guardas estruturais
# ------------------------------------------------------------------------------
test('duplicacao', bash, args: files('guard/check_duplication.sh'),
     workdir: raiz, suite: 'guard', timeout: 60)

# O host nao pode voltar a conhecer o fonte do modelo.
test('host-opaco', bash, args: files('guard/check_host_opaco.sh'),
     workdir: raiz, suite: 'guard', timeout: 60)

# Todo projeto de modelo tem tests/, docs/, README.md e CHANGELOG.md -- o que
# faz cada um autocontido e verificavel sozinho. Descobre os projetos por
# find, entao um modelo novo ja nasce cobrado.
test('modelo-estrutura', bash, args: files('guard/check_modelo_estrutura.sh'),
     workdir: raiz, suite: 'guard', timeout: 60)

# O meson do host nao tem aresta ate o .so do modelo (ele vem de dist/), entao
# o frescor tem de ser afirmado a mao -- senao a suite passa contra um plugin
# velho e a regressao no modelo fica invisivel.
test('modelo-fresco', bash, args: files('guard/check_modelo_fresco.sh'),
     workdir: raiz, suite: 'guard', timeout: 60)

# falcon1..4 (dentro do MESMO cenario) tem que declarar os mesmos slots, na
# mesma estrutura -- so os VALORES podem diferir (posicao/altitude/velocidade
# calibrados por aviao). Ver skeleton_diff.py para o metodo.
test('falcons-estrutura', bash, args: files('guard/check_falcons_estrutura.sh'),
     workdir: raiz, suite: 'guard', timeout: 60)
