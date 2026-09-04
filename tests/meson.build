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

# A derivacao de sementes de shared/xrandom -- fnv1a64/deriveSeed, puras, sem
# MIXR. Header-only, entao nao ha .cpp companheiro (ao contrario de
# test_domain_rules acima) -- so o include_directories ate shared/.
test_xrandom = executable('test-xrandom',
    files('domain/test_xrandom.cpp'),
    include_directories : include_directories('../shared'),
    dependencies        : [gtest_dep, gtest_main_dep],
    install             : false,
)
test('xrandom', test_xrandom, suite: 'domain', timeout: 60)

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

# shared/xrlbridge -- o contrato de dados com o treino (ordem canonica dos 28
# campos de observacao, a desnormalizacao de acao) e o mutex por tras de
# setPendingCommand/getPendingCommand/setObservation/getObservation. Nunca
# tinha teste direto nenhum antes deste arquivo -- so a contagem de campos,
# de relance, via a constante XRLBRIDGE_OBSERVATION_SIZE em test_xinfer.cpp.
test_xrlbridge = executable('test-xrlbridge',
    files('domain/test_xrlbridge.cpp'),
    dependencies        : [gtest_dep, gtest_main_dep, xrlbridge_dep, thread_dep],
    install             : false,
)
test('xrlbridge-contrato', test_xrlbridge, suite: 'domain', timeout: 60)

# shared/xjoystick -- so o que da para isolar sem Station/Simulation/
# AirVehicle/hardware de verdade: os setters de slot e o caminho de
# DEGRADACAO sem joystick fisico. As armadilhas de verdade (desengate dos
# hold modes do Autopilot, inversao do manete) continuam so validadas
# manualmente -- ver o comentario no topo do .cpp para o porque.
test_xjoystick = executable('test-xjoystick',
    files('domain/test_xjoystick.cpp'),
    link_args           : rpath_link_args,
    dependencies        : [gtest_dep, gtest_main_dep, xjoystick_dep, thread_dep],
    install             : false,
    build_rpath         : mixr_libdir,
)
test('xjoystick-degradacao', test_xjoystick, suite: 'domain', timeout: 60)

# ------------------------------------------------------------------------------
# Camadas de cenario, memoria e determinismo
#
# is_parallel: false em tudo que roda um executavel da poc: app/ScenarioTemplate
# grava o cenario expandido sempre no MESMO caminho.
# ------------------------------------------------------------------------------
# O quarto campo e o prefixo de rotulo: a src/poc/python-flight troca as folhas
# de ACAO da arvore por ( PyDecide ) e rotula cada uma com 'PY-<modo>'. As
# propriedades afirmadas sao as MESMAS -- quem evadiu avisa, quem apoiou
# recebeu, ninguem voou para dentro do terreno -- entao ela entra na bateria
# que ja existe, normalizando o rotulo na entrada, em vez de ganhar uma copia
# do runner que envelheceria em silencio ao lado dele.
pocs = [
    ['single-thread', single_thread,  'nao-critico', ''],
    ['multi-thread',  multi_thread,   'critico',     ''],
    ['python-flight', python_flight,  'python',      'PY-'],
]

scenario_runner = files('scenario/run_scenario_test.py')
leak_runner     = files('memory/run_leak_test.py')
determinism_sh  = files('determinism/check_determinism.sh')

foreach poc : pocs
    nome    = poc[0]
    bin     = poc[1]
    laco    = poc[2]
    prefixo = poc[3]

    foreach modo : ['intruder', 'lowfuel', 'terrain']
        test('scenario-' + modo + '-' + nome, py,
             args: scenario_runner + ['--binario', bin.full_path(), '--poc', nome,
                                      '--mode', modo, '--frames', '800',
                                      '--label-prefix', prefixo],
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

# A MESMA prova acima, agora para single-thread/multi-thread/bandit-dis -- ate
# a decima oitava passada do ./app, so ele tinha a ORDEM de encerramento
# corrigida (parar a thread T/C nativa ANTES do SHUTDOWN_EVENT); as tres pocs
# rodam a MESMA forma de laco de tempo real (createTimeCriticalProcess() +
# updateData() em laco continuo) e corriam o MESMO risco de auto-deadlock/
# crescimento de memoria documentado em xclock/ClockStation.hpp. Sem TUI/pty
# aqui -- ver o cabecalho do script para o porque.
#
# single-thread/multi-thread usam a porta 1234 (a mesma, uma poc por vez --
# is_parallel:false serializa com TODO o resto desta suite); bandit-dis usa a
# 1235.
test('scenario-single-thread-quit', py,
     args: files('scenario/run_poc_quit_test.py') + [
         '--binario', single_thread.full_path(), '--porta-tacview', '1234'],
     depends: single_thread, workdir: raiz, suite: 'scenario',
     timeout: 120, is_parallel: false)

test('scenario-multi-thread-quit', py,
     args: files('scenario/run_poc_quit_test.py') + [
         '--binario', multi_thread.full_path(), '--porta-tacview', '1234'],
     depends: multi_thread, workdir: raiz, suite: 'scenario',
     timeout: 120, is_parallel: false)

test('scenario-bandit-dis-quit', py,
     args: files('scenario/run_poc_quit_test.py') + [
         '--binario', bandit_dis.full_path(), '--porta-tacview', '1235'],
     depends: bandit_dis, workdir: raiz, suite: 'scenario',
     timeout: 120, is_parallel: false)

# As outras duas GEMEAS (ver tests/guard/check_duplication.sh) tambem levaram
# a mesma correcao -- include/app e src/app/RealTimeRun.cpp sao byte a byte
# identicos entre as quatro, entao o mesmo risco existia aqui tambem.
test('scenario-python-flight-quit', py,
     args: files('scenario/run_poc_quit_test.py') + [
         '--binario', python_flight.full_path(), '--porta-tacview', '1237'],
     depends: python_flight, workdir: raiz, suite: 'scenario',
     timeout: 120, is_parallel: false)

test('scenario-onnx-policy-quit', py,
     args: files('scenario/run_poc_quit_test.py') + [
         '--binario', onnx_policy.full_path(), '--porta-tacview', '1238'],
     depends: onnx_policy, workdir: raiz, suite: 'scenario',
     timeout: 120, is_parallel: false)

# '-threads'/'-deterministic' com um token nao numerico nao pode derrubar o
# processo com um sinal -- ver o cabecalho de run_bad_args_test.py e o fix em
# app/Options.cpp (parseLongOrDie/parseIntOrDie). bandit-dis fica de fora: nao
# tem app/Options.cpp (essa poc nao aceita CLI nenhuma, ver o cabecalho de
# src/main.cpp dela).
bad_args_runner = files('scenario/run_bad_args_test.py')
foreach par : [['single-thread', single_thread], ['multi-thread', multi_thread],
               ['python-flight', python_flight], ['onnx-policy', onnx_policy]]
    test('scenario-' + par[0] + '-bad-args', py,
         args: bad_args_runner + ['--binario', par[1].full_path()],
         depends: par[1], workdir: raiz, suite: 'scenario',
         timeout: 60, is_parallel: false)
endforeach

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

# ------------------------------------------------------------------------------
# src/poc/onnx-policy -- a poc em que a decisao E a rede neural.
#
# NAO entra na lista 'pocs' acima, e a razao e o desenho dela: a arvore tem uma
# folha util so, ( OnnxPolicy ), entao nao ha rotulo EVADE/SUPPORT/RTB para os
# modos 'intruder' e 'lowfuel' afirmarem -- uma politica treinada substitui a
# ARVORE INTEIRA, ao contrario da python-flight, que troca as FOLHAS e por isso
# preserva a forma que aqueles modos medem.
#
# O que sobra dela e afirmado por dois testes, e os dois dizem algo que nenhum
# outro diz:
#
#   1. a bateria de politica (a mesma de scenario-policy-*, agora apontada para
#      o cenario PROPRIO desta poc, sem troca de arvore): a rede decidiu em
#      todos os frames (bt=ONNX, ou seja nao caiu no Fallback), os dumps de 1, 2
#      e 4 threads sao byte-identicos com as quatro aeronaves inferindo em
#      paralelo, e 'dec' avanca na taxa de 'frame';
#   2. o modo 'terrain': com o piso do ( AltitudeSafetyBehavior vote: 90 ) acima
#      da altitude de cruzeiro, TODA linha sai bt=SAFETY -- a prova de que a
#      rede (voto 50) nao tem a ultima palavra sobre o terreno. E a propriedade
#      que torna seguro trocar de politica sem revalidar seguranca de voo.
# ------------------------------------------------------------------------------
test('scenario-onnx-policy', py,
     args: files('scenario/run_policy_test.py') + [
         '--binario', onnx_policy.full_path(),
         '--poc', 'onnx-policy', '--label', 'ONNX', '--frames', '600'],
     depends: onnx_policy, workdir: raiz, suite: 'scenario',
     timeout: 900, is_parallel: false)

test('scenario-terrain-onnx-policy', py,
     args: scenario_runner + ['--binario', onnx_policy.full_path(),
                              '--poc', 'onnx-policy', '--mode', 'terrain',
                              '--frames', '800'],
     depends: onnx_policy, workdir: raiz, suite: 'scenario',
     timeout: 900, is_parallel: false)

test('memory-onnx-policy', py,
     args: leak_runner + [onnx_policy.full_path(), 'onnx-policy'],
     depends: onnx_policy, workdir: raiz, suite: 'memory',
     timeout: 1800, is_parallel: false)

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
