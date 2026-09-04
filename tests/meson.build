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

# A logica pura por tras da animacao de fluxo da aba "Componentes" (F6) --
# app/ComponentFlowState.hpp/.cpp: ordem do ciclo de fases, avanco (com
# volta), e o relogio de animacao por CONTAGEM DE REDESENHOS (nao tempo de
# parede -- e um MODELO CONCEITUAL do ciclo do frame MIXR, nao uma medicao
# ao vivo, ver o comentario grande do header). Sem MIXR de verdade, sem
# FTXUI: ComponentTreeQuery.hpp so forward-declara mixr::simulation::Station.
test_app_component_flow = executable('test-app-component-flow-state',
    files('app/test_component_flow_state.cpp') + files(
        '../app/src/app/ComponentFlowState.cpp',
    ),
    include_directories : include_directories('../app/include'),
    dependencies        : [gtest_dep, gtest_main_dep],
    install             : false,
)
test('app-component-flow-state', test_app_component_flow, suite: 'domain', timeout: 60)

# A logica PURA da aba "Componentes" (F6) depois de ela virar VERTICAL e
# retratil: layout (profundidade -> linha, pai centrado sobre os filhos),
# conjunto de galhos RETRAIDOS (guardado por CHAVE, pra sobreviver a
# redescoberta da arvore a cada redesenho) e navegacao por teclado. Linka o
# ComponentTreePanel.cpp/ComponentTreeQuery.cpp de PRODUCAO (nao uma
# reimplementacao) -- dai o mixr_dep/ftxui_dep, mesma receita de
# 'app-map-canvas-fit'.
test_app_component_tree = executable('test-app-component-tree-layout',
    files('app/test_component_tree_layout.cpp') + files(
        '../app/src/app/ComponentTreePanel.cpp',
        '../app/src/app/ComponentTreeQuery.cpp',
        '../app/src/app/ComponentFlowState.cpp',
        '../app/src/app/FrameCallChain.cpp',
        '../app/src/app/FleetPanel.cpp',
    ),
    link_args           : rpath_link_args,
    include_directories : include_directories('../app/include'),
    dependencies        : [gtest_dep, gtest_main_dep, mixr_dep, xboard_dep, xplugin_dep, ftxui_dep],
    install             : false,
    build_rpath         : mixr_libdir,
)
test('app-component-tree-layout', test_app_component_tree, suite: 'domain', timeout: 60)

# A cadeia de chamadas do frame MIXR que a aba F6 mostra -- nomes, argumentos
# e o dt que desce dividido por 4 e volta multiplicado por 4 -- mais a janela
# de rolagem que mantem a linha da fase corrente visivel. Sem MIXR, sem FTXUI.
test_app_call_chain = executable('test-app-frame-call-chain',
    files('app/test_frame_call_chain.cpp') + files(
        '../app/src/app/FrameCallChain.cpp',
    ),
    include_directories : include_directories('../app/include'),
    dependencies        : [gtest_dep, gtest_main_dep],
    install             : false,
)
test('app-frame-call-chain', test_app_call_chain, suite: 'domain', timeout: 60)

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
# O SEGUNDO campo era o binario de cada poc; hoje e SEMPRE 'app' -- as pocs
# deixaram de ter executavel proprio quando o ./app virou o runner unico (ver
# src/poc/meson.build). O que distingue uma poc da outra e a fixture, gerada
# por make_fixture.py a partir do cenario dela, e passada com '-f'.
pocs = [
    ['single-thread', app, 'nao-critico', ''],
    ['multi-thread',  app, 'critico',     ''],
    ['python-flight', app, 'python',      'PY-'],
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

# A MESMA prova, agora num cenario COM 'networks:'. Nao e redundante com a de
# cima: os tres cenarios proprios do ./app sao hermeticos, e por isso a TUI
# nunca tinha aberto uma Station com lista de redes -- foi exatamente ai que
# tres over-unref de Station::getNetworks() (que, ao contrario de
# getPlayers(), NAO e pre-ref()'d) ficaram latentes ate o ./app virar o runner
# das pocs de DIS. Os sintomas eram SIGSEGV em List::Item::getValue() e
# 'ExpInvalidRefCount' dentro da propria Station. Este teste e o que trava a
# regressao.
test('scenario-app-quit-dis', py,
     args: files('scenario/run_app_quit_test.py') + [
         '--binario', app.full_path(), '--scenario', 'multi-thread'],
     depends: app, workdir: raiz, suite: 'scenario',
     timeout: 300, is_parallel: false)

# NAO HA MAIS um teste de saida POR POC. Ate o ./app virar o runner unico,
# cada poc tinha o proprio executavel com uma copia byte a byte do laco de
# tempo real, e cada copia corria o mesmo risco de auto-deadlock/crescimento de
# memoria documentado em xclock/ClockStation.hpp -- dai cinco testes quase
# iguais. Hoje o laco e UM, o do ./app, e quem o cobre e o 'scenario-app-quit'
# acima (com pty, porque agora ha TUI e dialogo de confirmacao). Rodar o mesmo
# binario cinco vezes com cenarios diferentes nao acrescentaria cobertura
# nenhuma sobre a ordem de encerramento, que e o que aquele teste afirma.

# '-threads'/'-deterministic' com um token nao numerico nao pode derrubar o
# processo com um sinal -- ver o cabecalho de run_bad_args_test.py e o fix em
# app/src/app/Options.cpp (parseLongOrDie/parseIntOrDie). Era um teste por
# poc; hoje ha um binario so, entao ha um teste so.
bad_args_runner = files('scenario/run_bad_args_test.py')
test('scenario-bad-args', py,
     args: bad_args_runner + ['--binario', app.full_path()],
     depends: app, workdir: raiz, suite: 'scenario',
     timeout: 60, is_parallel: false)

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
             '--binario', app.full_path(),
             '--tree', caso[1], '--label', caso[2], '--frames', '600'],
         depends: app, workdir: raiz, suite: 'scenario',
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
         '--binario', app.full_path(),
         '--poc', 'onnx-policy', '--label', 'ONNX', '--frames', '600'],
     depends: app, workdir: raiz, suite: 'scenario',
     timeout: 900, is_parallel: false)

test('scenario-terrain-onnx-policy', py,
     args: scenario_runner + ['--binario', app.full_path(),
                              '--poc', 'onnx-policy', '--mode', 'terrain',
                              '--frames', '800'],
     depends: app, workdir: raiz, suite: 'scenario',
     timeout: 900, is_parallel: false)

test('memory-onnx-policy', py,
     args: leak_runner + [app.full_path(), 'onnx-policy'],
     depends: app, workdir: raiz, suite: 'memory',
     timeout: 1800, is_parallel: false)

# multi-thread NAO. Ver o cabecalho do script.
test('onde-a-decisao-roda', py,
     args: files('determinism/check_onde_a_decisao_roda.py') + [
         '--single', app.full_path(),
         '--multi', app.full_path()],
     depends: app,
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
         '--binario', app.full_path(), '--poc', 'single-thread',
         '--bad-nosym', bad_plugins['nosym'].full_path(),
         '--bad-abi', bad_plugins['abi'].full_path(),
         '--bad-collide', bad_plugins['collide'].full_path()],
     depends: [app, bad_plugins['nosym'], bad_plugins['abi'], bad_plugins['collide']],
     workdir: raiz, suite: 'plugin', timeout: 900, is_parallel: false)

test('plugin-hotswap', py,
     args: files('plugin/run_plugin_hotswap.py') + [
         '--binario', app.full_path(), '--poc', 'single-thread',
         '--so-a', model_build / 'libmodel_variant_a.so',
         '--so-b', model_build / 'libmodel_variant_b.so'],
     depends: app, workdir: raiz, suite: 'plugin',
     timeout: 900, is_parallel: false)

# O MODELO DESCONHECIDO -- a prova que o resto nao da.
#
# Roda o cenario de PRODUCAO trocando so o 'file:' do ( PluginModule ) por um
# modelo escrito SO contra o SDK publicado, sem olhar domain/, bt/ ou ubf/.
# E o unico teste que pode falhar por "o contrato nao basta".
test('plugin-modelo-estranho', py,
     args: files('plugin/run_stub_model.py') + [
         '--binario', app.full_path(), '--poc', 'single-thread',
         '--stub', dist_plugins / 'libstub.so'],
     depends: app, workdir: raiz, suite: 'plugin',
     timeout: 900, is_parallel: false)

# O DEPOSITO DE TERCEIRO -- prova que models/plugins/ (o deposito pra um .so
# ja compilado fora deste repositorio, ver models/plugins/README.md) nao e
# so uma pasta com um README: um .so colocado ali e de fato copiado pra
# dist/lib/mixr-plugins/ (o mesmo passo que o alvo 'models' do Makefile
# raiz faz) e o resultado E USAVEL numa simulacao de verdade -- reusa o
# stub como o "terceiro" e a MESMA bateria de plugin-modelo-estranho.
test('plugin-deposito-terceiro', py,
     args: files('plugin/run_thirdparty_deposit.py') + [
         '--binario', app.full_path(), '--poc', 'single-thread',
         '--source-so', dist_plugins / 'libstub.so',
         '--run-stub-model', raiz / 'tests' / 'plugin' / 'run_stub_model.py'],
     depends: app, workdir: raiz, suite: 'plugin',
     timeout: 900, is_parallel: false)

# ------------------------------------------------------------------------------
# Guardas estruturais
# ------------------------------------------------------------------------------
# A guarda 'duplicacao' foi APOSENTADA aqui. Ela existia para travar um
# invariante que nao existe mais: cada poc carregava uma copia byte a byte da
# camada de aplicacao (~1.500 linhas), e a guarda garantia que as copias nao
# divergissem. Com o ./app virando o runner unico, as pocs deixaram de ter
# codigo -- sao 'configs/' + 'data/' + README. Nao ha mais duas copias para
# comparar, e uma guarda que nao acha nada para comparar ou falha (o caso
# deste script, que sai 1 quando o find volta vazio) ou passa vacuamente, que
# e pior. A duplicacao esta dissolvida POR CONSTRUCAO, como aconteceu com o
# MODELO quando ele saiu para models/flight.

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
