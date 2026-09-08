# ==============================================================================
# Suite do HOST -- so entra no build com -Dtests=true (ver meson_options.txt)
#
# As camadas 'domain' e 'tree' NAO estao mais aqui: elas compilam domain/ e bt/
# direto, e esses fontes sairam para models/players/A-4/. Moraram junto com o que
# testam -- rode 'make test-models' (ou 'make test', que roda as duas).
#
#   domain-rules  as primitivas de deteccao do libs/xmsg (host, ficaram)
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
# 'models/players/A-4' e um projeto Meson AUTOCONTIDO (ver CLAUDE.md, secao
# "O MODELO e um plugin, construido numa etapa PREVIA") -- constroi em
# './build' DENTRO da propria pasta, nao mais em
# 'build-flight' na raiz (esse era o caminho do design anterior, quando
# 'make models' fazia o setup do flight direto por aqui). As variantes A/B
# de teste ('libmodel_variant_a.so'/'_b.so', para plugin-hotswap) saem la.
model_build  = raiz / 'models' / 'players' / 'A-4' / 'build'

# ------------------------------------------------------------------------------
# Camada 1 -- as primitivas de deteccao do libs/xmsg
#
# Sao do HOST (libs/xmsg), nao do modelo, entao ficaram. Tao livres de MIXR
# quanto o domain/ que foi embora: regra pura, milissegundos.
# ------------------------------------------------------------------------------
xmsg_rules_dir = raiz / 'libs' / 'xmsg'
test_domain_rules = executable('test-domain-rules',
    files('domain/test_xmsg_rules.cpp') + files(
        xmsg_rules_dir / 'rules/Deadband.cpp',
        xmsg_rules_dir / 'rules/EmitGate.cpp',
        xmsg_rules_dir / 'rules/RateWindow.cpp',
        xmsg_rules_dir / 'rules/Schmitt.cpp',
    ),
    include_directories : include_directories('../libs'),
    dependencies        : [gtest_dep, gtest_main_dep],
    install             : false,
)
test('domain-rules', test_domain_rules, suite: 'domain', timeout: 60)

# RecordWriter (libs/xmsg) -- monta NDJSON num buffer fixo, sem alocar.
# Achado por auditoria: addLabel() nao escapava o valor, permitindo injecao
# de JSON via nome de player recebido por DIS (ver o cabecalho do teste).
test_record_writer = executable('test-record-writer',
    files('domain/test_record_writer.cpp') + files(
        xmsg_rules_dir / 'RecordWriter.cpp',
    ),
    include_directories : include_directories('../libs'),
    dependencies        : [gtest_dep, gtest_main_dep],
    install             : false,
)
test('record-writer', test_record_writer, suite: 'domain', timeout: 60)

# A derivacao de sementes de libs/xrandom -- fnv1a64/deriveSeed, puras, sem
# MIXR. Header-only, entao nao ha .cpp companheiro (ao contrario de
# test_domain_rules acima) -- so o include_directories ate libs/.
test_xrandom = executable('test-xrandom',
    files('domain/test_xrandom.cpp'),
    include_directories : include_directories('../libs'),
    dependencies        : [gtest_dep, gtest_main_dep],
    install             : false,
)
test('xrandom', test_xrandom, suite: 'domain', timeout: 60)

# A logica pura (sem MIXR, sem FTXUI) por tras da aba "Memoria" do dashboard
# (./app/, fora de src/ -- ver o cabecalho de ../app/include/app/
# MetaObjectSnapshot.hpp). Mesmo motivo de estar na suite 'domain': regra
# isolada, milissegundos.
test_app_meta = executable('test-app-meta',
    files('../app/tests/test_meta_object_snapshot.cpp') + files(
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
    files('../app/tests/test_map_geometry.cpp') + files(
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
    files('../app/tests/test_map_canvas_fit.cpp') + files(
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

# O buffer em memoria de libs/xlog -- a fonte da aba "Log" do ./app -- e
# as duas regras puras do painel (filtro por nivel minimo e o ciclo dele).
# O buffer e estado GLOBAL do processo de proposito: e o que faz o LOG(...)
# do plugin do modelo (um .so aberto por dlopen) cair no MESMO lugar que o
# do host, sem nenhuma ponte extra.
test_app_log = executable('test-app-log',
    files('../app/tests/test_log_panel.cpp') + files(
        '../app/src/app/LogPanel.cpp',
    ),
    link_args           : rpath_link_args,
    include_directories : include_directories('../app/include'),
    dependencies        : [gtest_dep, gtest_main_dep, xlog_dep, ftxui_dep],
    install             : false,
    build_rpath         : mixr_libdir,
)
test('app-log', test_app_log, suite: 'domain', timeout: 60)

# 'discoverFolderScenarios()' (app/ScenarioFolder.hpp) -- a descoberta em
# disco por tras de './app -folder <pasta>'. So std::filesystem, sem
# MIXR/FTXUI -- daí nao ter mixr_dep/ftxui_dep aqui, unico teste de app/ tao
# enxuto quanto os de tests/domain/.
test_app_scenario_folder = executable('test-app-scenario-folder',
    files('../app/tests/test_scenario_folder.cpp') + files(
        '../app/src/app/ScenarioFolder.cpp',
    ),
    include_directories : include_directories('../app/include'),
    dependencies        : [gtest_dep, gtest_main_dep],
    install             : false,
)
test('app-scenario-folder', test_app_scenario_folder, suite: 'domain', timeout: 60)

# A logica pura por tras da animacao de fluxo da aba "Componentes" (F6) --
# app/ComponentFlowState.hpp/.cpp: ordem do ciclo de fases, avanco (com
# volta), e o relogio de animacao por CONTAGEM DE REDESENHOS (nao tempo de
# parede -- e um MODELO CONCEITUAL do ciclo do frame MIXR, nao uma medicao
# ao vivo, ver o comentario grande do header). Sem MIXR de verdade, sem
# FTXUI: ComponentTreeQuery.hpp so forward-declara mixr::simulation::Station.
test_app_component_flow = executable('test-app-component-flow-state',
    files('../app/tests/test_component_flow_state.cpp') + files(
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
    files('../app/tests/test_component_tree_layout.cpp') + files(
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
    files('../app/tests/test_frame_call_chain.cpp') + files(
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
    files('../app/tests/test_breakpoint_controller.cpp') + files(
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
    files('../app/tests/test_speed_ladder.cpp') + files(
        '../app/src/app/SpeedLadder.cpp',
    ),
    link_args           : rpath_link_args,
    include_directories : include_directories('../app/include'),
    dependencies        : [gtest_dep, gtest_main_dep],
    install             : false,
    build_rpath         : mixr_libdir,
)
test('app-speed-ladder', test_app_speed_ladder, suite: 'domain', timeout: 60)

# A parte sem FTXUI da aba "EDL" (F7) -- io de arquivo (nunca sobre o
# '.edl.in'/'.generated.edl' de producao, so sobre editedScenarioPath()) e a
# interpretacao do exit code do oraculo 'edlcheck'. Sem MIXR, sem FTXUI --
# 'runEdlCheck()' recebe o caminho do binario por parametro, entao o teste
# roda contra /bin/true//bin/false/bin/echo, nunca contra o 'edlcheck' real.
test_app_edl_editor = executable('test-app-edl-editor-state',
    files('../app/tests/test_edl_editor_state.cpp') + files(
        '../app/src/app/EdlEditorState.cpp',
    ),
    include_directories : include_directories('../app/include'),
    dependencies        : [gtest_dep, gtest_main_dep],
    install             : false,
)
test('app-edl-editor-state', test_app_edl_editor, suite: 'domain', timeout: 60)

# O destaque de sintaxe .edl da aba "EDL" (F7) -- MESMA gramatica/precedencia
# de src/ui/edl_builder_core.js::tokenizeEdlText(), a que ja' colore a
# "Pre-visualizacao .edl" do editor grafico web. Sem MIXR, sem FTXUI -- so'
# string in, tokens out (a parte que COLORE, com Color/Element de verdade,
# fica em app/src/app/DashboardLoop.cpp, junto do resto do desenho da aba).
test_app_edl_syntax = executable('test-app-edl-syntax-highlight',
    files('../app/tests/test_edl_syntax_highlight.cpp') + files(
        '../app/src/app/EdlSyntaxHighlight.cpp',
    ),
    include_directories : include_directories('../app/include'),
    dependencies        : [gtest_dep, gtest_main_dep],
    install             : false,
)
test('app-edl-syntax-highlight', test_app_edl_syntax, suite: 'domain', timeout: 60)

# A parte COM FTXUI do mesmo destaque -- cor + o corte do glifo sob o cursor
# (app/EdlHighlightRender.cpp, extraida de dentro de DashboardLoop.cpp por
# auditoria: ficava intestavel sem levantar a TUI inteira, e escondia um bug
# de UTF-8 -- ver o cabecalho do .cpp). Sem MIXR -- so' FTXUI, renderizado de
# verdade via Screen::ToString() (mesmo padrao de test_memory_panel.cpp).
test_app_edl_highlight_render = executable('test-app-edl-highlight-render',
    files('../app/tests/test_edl_highlight_render.cpp') + files(
        '../app/src/app/EdlHighlightRender.cpp',
        '../app/src/app/EdlSyntaxHighlight.cpp',
    ),
    link_args           : rpath_link_args,
    include_directories : include_directories('../app/include'),
    dependencies        : [gtest_dep, gtest_main_dep, ftxui_dep],
    install             : false,
    build_rpath         : mixr_libdir,
)
test('app-edl-highlight-render', test_app_edl_highlight_render, suite: 'domain', timeout: 60)

# app::parseCommandLine() -- traducao de argv para Options, incluindo os
# die() de token invalido/flag sem valor/'-deterministic' <= 0 (via death
# test -- e o unico jeito de exercitar um std::exit() sem matar a suite).
test_app_options = executable('test-app-options',
    files('../app/tests/test_options.cpp') + files(
        '../app/src/app/Options.cpp',
    ),
    include_directories : include_directories('../app/include'),
    dependencies        : [gtest_dep, gtest_main_dep],
    install             : false,
)
test('app-options', test_app_options, suite: 'domain', timeout: 60)

# app::ensureTerrainData()/ensureAllTerrainTiles() -- '.hgt.gz' corrompido/
# truncado/ausente. Achado por auditoria como buraco de cobertura: a
# validacao de tamanho ja era robusta no codigo (isValidSrtmSize(), die()
# com mensagem clara, ensureAllTerrainTiles() tolerante), so faltava o
# teste. Roda gzip/gunzip de verdade num diretorio temporario -- ver o
# cabecalho do arquivo.
test_app_terrain_data = executable('test-app-terrain-data',
    files('../app/tests/test_terrain_data.cpp') + files(
        '../app/src/app/TerrainData.cpp',
    ),
    include_directories : include_directories('../app/include'),
    dependencies        : [gtest_dep, gtest_main_dep],
    install             : false,
)
test('app-terrain-data', test_app_terrain_data, suite: 'domain', timeout: 60)

# app::falconFleet()/app::adHocScenario() -- a entrada de '-f <arquivo>'.
test_app_adhoc_scenario = executable('test-app-adhoc-scenario',
    files('../app/tests/test_adhoc_scenario.cpp') + files(
        '../app/src/app/AdHocScenario.cpp',
    ),
    include_directories : include_directories('../app/include'),
    dependencies        : [gtest_dep, gtest_main_dep],
    install             : false,
)
test('app-adhoc-scenario', test_app_adhoc_scenario, suite: 'domain', timeout: 60)

# app::generateScenario() -- '.edl.in' -> '.edl' (substituicao do
# '@NUM_TC_THREADS@'/'extraTokens', o aviso de token sem par, e os die() de
# leitura de arquivo -- template ausente, '@include:' malformado ou
# apontando pra um fragmento inexistente).
test_app_scenario_template = executable('test-app-scenario-template',
    files('../app/tests/test_scenario_template.cpp') + files(
        '../app/src/app/ScenarioTemplate.cpp',
    ),
    include_directories : include_directories('../app/include'),
    dependencies        : [gtest_dep, gtest_main_dep],
    install             : false,
)
test('app-scenario-template', test_app_scenario_template, suite: 'domain', timeout: 60)

# O parser minimo de XML do BT.CPP (app/BehaviorTreeView.cpp) e as duas
# regras puras que o consomem (matchesLabel()/flattenBehaviorTree()) -- ate
# agora so linkado por outros testes (map-canvas-fit/component-tree-layout)
# para satisfazer o link de MapPanel.cpp/ComponentTreePanel.cpp, nunca
# exercitado diretamente.
test_app_bt_view = executable('test-app-behavior-tree-view',
    files('../app/tests/test_behavior_tree_view.cpp') + files(
        '../app/src/app/BehaviorTreeView.cpp',
    ),
    include_directories : include_directories('../app/include'),
    dependencies        : [gtest_dep, gtest_main_dep],
    install             : false,
)
test('app-behavior-tree-view', test_app_bt_view, suite: 'domain', timeout: 60)

# app/FleetPanel.cpp -- cor/rotulo de lado, glifo de especie, rotulo/cor de
# modo, cor de comportamento, e a formatacao das linhas/detalhe da aba
# Players. Precisa de mixr_dep so pelas CONSTANTES de enum (BLUE/AIR_VEHICLE/
# ACTIVE/...), nunca de uma Station de verdade -- mesma razao pela qual
# FleetPanel.cpp ja aparecia (sem teste proprio) em app-map-canvas-fit/
# app-component-tree-layout.
test_app_fleet_panel = executable('test-app-fleet-panel',
    files('../app/tests/test_fleet_panel.cpp') + files(
        '../app/src/app/FleetPanel.cpp',
    ),
    link_args           : rpath_link_args,
    include_directories : include_directories('../app/include'),
    dependencies        : [gtest_dep, gtest_main_dep, mixr_dep, xplugin_dep, ftxui_dep],
    install             : false,
    build_rpath         : mixr_libdir,
)
test('app-fleet-panel', test_app_fleet_panel, suite: 'domain', timeout: 60)

# app/MemoryPanel.cpp -- classRowText()/renderClassRow(), a linha da aba
# Memoria. ClassStat (app/MetaObjectSnapshot.hpp) e struct pura; a unica
# dependencia real desta unidade e FTXUI.
test_app_memory_panel = executable('test-app-memory-panel',
    files('../app/tests/test_memory_panel.cpp') + files(
        '../app/src/app/MemoryPanel.cpp',
    ),
    link_args           : rpath_link_args,
    include_directories : include_directories('../app/include'),
    dependencies        : [gtest_dep, gtest_main_dep, ftxui_dep],
    install             : false,
    build_rpath         : mixr_libdir,
)
test('app-memory-panel', test_app_memory_panel, suite: 'domain', timeout: 60)

# app/BackgroundPanel.cpp -- a aba "Tempo Nao-Critico" (F4). BackgroundInfo
# (app/DashboardState.hpp) e struct pura; a unica dependencia real desta
# unidade e FTXUI.
test_app_background_panel = executable('test-app-background-panel',
    files('../app/tests/test_background_panel.cpp') + files(
        '../app/src/app/BackgroundPanel.cpp',
    ),
    link_args           : rpath_link_args,
    include_directories : include_directories('../app/include'),
    dependencies        : [gtest_dep, gtest_main_dep, ftxui_dep],
    install             : false,
    build_rpath         : mixr_libdir,
)
test('app-background-panel', test_app_background_panel, suite: 'domain', timeout: 60)

# A regra de selecao de pista (filtro por lado + desempate deterministico)
# de libs/xtrack, separada da travessia AirVehicle->OnboardComputer->
# TrackManager->Track que a alimenta -- ver o "porque" em
# libs/xtrack/TrackQuery.hpp. Alimenta models/players/A-4/src/ubf/
# FlightState.cpp (a percepcao do modelo).
test_track_selection = executable('test-track-selection',
    files('domain/test_track_selection.cpp'),
    link_args           : rpath_link_args,
    dependencies        : [gtest_dep, gtest_main_dep, xtrack_dep],
    install             : false,
    build_rpath         : mixr_libdir,
)
test('track-selection', test_track_selection, suite: 'domain', timeout: 60)

# A UNICA responsabilidade de libs/xboard -- ser um mapa seguro entre N
# escritores concorrentes (o pool T/C, um por player) e 1 leitor (o laco
# de background) -- ver o "porque" de ser a UNICA shared_library() de
# libs/ em xboard/Board.hpp. Nunca tinha teste direto antes desta
# suite: so validacao indireta pelos dumps end-to-end.
test_xboard_concurrency = executable('test-xboard-concurrency',
    files('domain/test_xboard_concurrency.cpp'),
    link_args           : rpath_link_args,
    dependencies        : [gtest_dep, gtest_main_dep, xboard_dep, thread_dep],
    install             : false,
    build_rpath         : mixr_libdir,
)
test('xboard-concurrency', test_xboard_concurrency, suite: 'domain', timeout: 60)

# libs/xinfer -- a DEGRADACAO do motor de inferencia: modelo ausente,
# arquivo que nao e .onnx, id invalido. Nenhuma dessas entradas pode derrubar
# a simulacao no meio do frame; todas tem de devolver 0/negativo e deixar o
# consumidor cair no ramo seguinte da arvore. O caminho FELIZ (inferencia de
# verdade + determinismo com 1/2/4 threads) precisa de um .onnx e vem depois.
# O .onnx DE VERDADE que models/players/A-4 instala -- por caminho, nao uma copia.
# Mesmo padrao de FLIGHT_TREE_XML em models/players/A-4/tests/meson.build: um teste
# contra uma copia provaria que a copia esta certa, o que nao interessa.
policy_onnx = raiz / 'models' / 'players' / 'A-4' / 'configs' / 'policy_example.onnx'

test_xinfer = executable('test-xinfer',
    files('domain/test_xinfer.cpp'),
    cpp_args            : ['-DPOLICY_ONNX="' + policy_onnx + '"'],
    link_args           : rpath_link_args,
    dependencies        : [gtest_dep, gtest_main_dep, xinfer_dep, xrlbridge_dep, thread_dep],
    install             : false,
    build_rpath         : mixr_libdir,
)
test('xinfer-degradacao', test_xinfer, suite: 'domain', timeout: 120)

# libs/xpyembed -- degradacao E isolamento por chave. O segundo e o que
# mantem o determinismo da poc python-flight verde: a ordem de aquisicao do
# GIL nao e deterministica, entao dois avioes rodando o mesmo script NAO
# podem compartilhar globais.
test_xpyembed = executable('test-xpyembed',
    files('domain/test_xpyembed.cpp'),
    link_args           : rpath_link_args,
    dependencies        : [gtest_dep, gtest_main_dep, xpyembed_dep, thread_dep],
    install             : false,
    build_rpath         : mixr_libdir,
)
test('xpyembed-isolamento', test_xpyembed, suite: 'domain', timeout: 120)

# libs/xrlbridge -- o contrato de dados com o treino (ordem canonica dos 28
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

# libs/xjoystick -- so o que da para isolar sem Station/Simulation/
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
#
# >>> ESTA LISTA NAO E O PONTO DE ENTRADA PADRAO PRA UM MODELO/POC NOVO. <<<
# So entra aqui quem segue o formato dual 'intruder'/'lowfuel' com rotulos
# EVADE/SUPPORT/RTB (semantica de interceptacao aerea, tipo A-4/flight). Um
# modelo novo que nao decide nesse formato (a maioria dos que vem por ai --
# ver TODO.md) NAO deve ser forcado aqui; ele ganha cobertura por um bloco
# test() manual (precedente: 'onnx-policy', mais abaixo neste arquivo) ou
# nenhuma cobertura em tests/meson.build (precedente: 'built-in_mixr_1',
# que so tem um 'make check-<nome>' no Makefile raiz). A arvore de decisao
# completa esta em CONTRIBUTING.md secao 5.3 -- leia antes de editar esta
# lista OU de escrever um bloco test() novo.
pocs = [
    ['flight',        app, 'flight',      ''],
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

    # 'terrain' EXISTIU aqui (afirmava que o AltitudeSafetyBehavior, voto 90,
    # ganhava do BtBehavior, voto 50) e foi REMOVIDO junto com o arbitro --
    # os cenarios de producao nao instanciam mais ( UbfArbiter )/
    # ( AltitudeSafetyBehavior ), entao 'bt=SAFETY' e inalcancavel hoje. Ver
    # a secao "SEM ARBITRO" no topo de cada scenario.edl.in.
    foreach modo : ['intruder', 'lowfuel']
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

# ACHADO POR AUDITORIA, CORRIGIDO (nao redescobrir): este script (prova que a
# sub-semente de jitter da patrulha deriva do NOME do player e nao da ordem
# de descoberta entre threads T/C -- ver libs/xrandom no CLAUDE.md) sempre
# existiu, rodava e passava (confirmado manualmente), mas nunca era
# exercitado por 'make test'/CI -- ficava ORFAO. Uma regressao nessa
# propriedade (ex.: a derivacao de semente voltar a depender de ordem)
# passaria despercebida indefinidamente. Registrado so' para 'flight' -- a
# propriedade testada (domain::PatrolPlan) mora inteira no MODELO
# compartilhado (nao e' especifica de host/arvore), entao testar uma vez
# basta, sem dobrar o tempo desta suite ja lenta.
patrol_seed_sh = files('determinism/check_patrol_seed.sh')
test('patrol-seed-determinism', bash,
     args: patrol_seed_sh + [app.full_path(), 'flight', 'flight', '300'],
     depends: app, workdir: raiz, suite: 'determinism',
     timeout: 1800, is_parallel: false)

# NAO HA MAIS um teste 'scenario-adhoc-fleet' aqui: ele provava que
# adHocScenario() ('-f <arquivo>') casava o caminho contra um catalogo
# estatico de cenarios antes de assumir falcon1..4 -- esse catalogo foi
# removido (ver CLAUDE.md). '-f' hoje SEMPRE assume falcon1..4; um cenario
# com frota diferente (ex.: full-systems-nav, frota {"a4"}) deve ser
# carregado por '-folder <pasta>' em vez de '-f' -- a mesma propriedade que
# este teste defendia (full-systems-nav com a frota certa) continua coberta
# por 'scenario-folder' logo abaixo, que carrega o MESMO .edl.in via
# '-folder'.

# './app -folder <pasta>' -- a navegacao por pasta de cenarios de sandbox
# (app/ScenarioFolder.hpp). Ponta a ponta pelo caminho NAO-INTERATIVO
# ('-folder' + '-scenario <subpasta>'), que exercita a descoberta em disco,
# a selecao direta sem UI e app::discoverFleet() (a frota generica que
# cenarios de '-folder' usam -- ver o comentario em app/Fleet.hpp). Ver o
# cabecalho do script.
test('scenario-folder', py,
     args: files('scenario/run_scenario_folder_test.py') + [
         '--binario', app.full_path(), '--frames', '50'],
     depends: app, workdir: raiz, suite: 'scenario',
     timeout: 300, is_parallel: false)

# DOIS processos './app' competindo pela MESMA porta Tacview ao mesmo tempo --
# achado por auditoria como buraco de cobertura: o codigo ja degrada
# graciosamente (RealtimeTelemetryServer::start() so loga e devolve false se
# o bind() falhar), mas nenhum teste subia dois processos DE VERDADE
# competindo por ela (toda a suite roda 'is_parallel: false', nunca dois
# testes de poc ao mesmo tempo). Este teste sobe os dois DENTRO de si mesmo,
# de proposito. Ver o cabecalho do script.
test('dual-tacview-port', py,
     args: files('scenario/run_dual_tacview_port_test.py') + [
         '--binario', app.full_path(), '--frames', '100'],
     depends: app, workdir: raiz, suite: 'scenario',
     timeout: 300, is_parallel: false)

# PDU DIS malformado/truncado, enviado cru por UDP contra o cenario de
# PRODUCAO (o UNICO venue que abre a porta DIS de verdade -- toda fixture de
# make_fixture.py remove 'networks:' de proposito). mixr::dis::NetIO e
# dependencia BINARIA -- este teste so pode documentar/travar o
# comportamento observado (nao crasha, hoje), nunca "consertar" o parser.
# Ver o cabecalho do script para o que foi lido no fonte vendorizado antes de
# escrever isto (offsetof() real contra os headers do Conan, nao adivinhado).
test('dis-pdu-malformado', py,
     args: files('scenario/run_dis_malformed_pdu_test.py') + [
         '--binario', app.full_path()],
     depends: app, workdir: raiz, suite: 'scenario',
     timeout: 120, is_parallel: false)

# A SAIDA do app, no caminho INTERATIVO -- o unico que os testes acima nao
# alcancam: eles rodam '-deterministic', que nao tem TUI nem thread de tempo
# critico nativa. Este sobe o binario num pty de verdade, aperta [q] e afirma
# que o processo TERMINA. Inclui o caso que de fato travava: um cliente na
# porta do Tacview que conecta e para de ler. Ver o cabecalho do script.
test('scenario-app-quit', py,
     args: files('scenario/run_app_quit_test.py') + [
         '--binario', app.full_path(), '--scenario', 'onnx-policy'],
     depends: app, workdir: raiz, suite: 'scenario',
     timeout: 300, is_parallel: false)

# A MESMA prova, agora num cenario COM 'networks:'. Nao e redundante com a de
# cima: o cenario acima e hermetico, e por isso a TUI
# nunca tinha aberto uma Station com lista de redes -- foi exatamente ai que
# tres over-unref de Station::getNetworks() (que, ao contrario de
# getPlayers(), NAO e pre-ref()'d) ficaram latentes ate o ./app virar o runner
# das pocs de DIS. Os sintomas eram SIGSEGV em List::Item::getValue() e
# 'ExpInvalidRefCount' dentro da propria Station. Este teste e o que trava a
# regressao.
test('scenario-app-quit-dis', py,
     args: files('scenario/run_app_quit_test.py') + [
         '--binario', app.full_path(), '--scenario', 'flight'],
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

# >>> BLOCO test() MANUAL PRA MODELO/CENARIO NOVO ENTRA POR AQUI (ou logo
# abaixo) -- este e o padrao pra quem NAO serve a lista 'pocs' acima (ver o
# comentario junto dela e CONTRIBUTING.md secao 5.3). O bloco de
# 'scenario-onnx-policy'/'memory-onnx-policy' logo a seguir e o precedente a
# copiar: mesmo scenario_runner/leak_runner, so que fora do 'foreach', com o
# '--poc'/'--label' do SEU cenario. <<<
# ------------------------------------------------------------------------------
# src/poc/onnx-policy -- a poc em que a decisao E a rede neural.
#
# NAO entra na lista 'pocs' acima, e a razao e o desenho dela: a arvore tem uma
# folha util so, ( OnnxPolicy ), entao nao ha rotulo EVADE/SUPPORT/RTB para os
# modos 'intruder' e 'lowfuel' afirmarem -- uma politica treinada substitui a
# ARVORE INTEIRA, ao contrario da python-flight, que troca as FOLHAS e por isso
# preserva a forma que aqueles modos medem.
#
# O que sobra dela e afirmado pela bateria de politica (a mesma de
# scenario-policy-*, agora apontada para o cenario PROPRIO desta poc, sem
# troca de arvore): a rede decidiu em todos os frames (bt=ONNX, ou seja nao
# caiu no Fallback), os dumps de 1, 2 e 4 threads sao byte-identicos com as
# quatro aeronaves inferindo em paralelo, e 'dec' avanca na taxa de 'frame'.
#
# HAVIA um segundo teste aqui, 'scenario-terrain-onnx-policy' (modo
# 'terrain': com o piso do ( AltitudeSafetyBehavior vote: 90 ) acima da
# altitude de cruzeiro, TODA linha saia bt=SAFETY -- a prova de que a rede,
# voto 50, nao tinha a ultima palavra sobre o terreno). Foi REMOVIDO junto
# com o arbitro: o cenario de producao desta poc nao instancia mais
# ( UbfArbiter )/( AltitudeSafetyBehavior ), entao bt=SAFETY e inalcancavel
# hoje -- ver a secao "SEM ARBITRO" no topo de
# src/poc/onnx-policy/configs/scenario.edl.in. Trocar de politica aqui volta
# a exigir revalidar seguranca de voo por conta propria.
# ------------------------------------------------------------------------------
test('scenario-onnx-policy', py,
     args: files('scenario/run_policy_test.py') + [
         '--binario', app.full_path(),
         '--poc', 'onnx-policy', '--label', 'ONNX', '--frames', '600'],
     depends: app, workdir: raiz, suite: 'scenario',
     timeout: 900, is_parallel: false)

test('memory-onnx-policy', py,
     args: leak_runner + [app.full_path(), 'onnx-policy'],
     depends: app, workdir: raiz, suite: 'memory',
     timeout: 1800, is_parallel: false)

# O CONTROLE NEGATIVO da suite 'memory' -- prova que 'memory-<poc>' (acima)
# PEGARIA um vazamento de verdade, nao so que a producao de hoje esta limpa.
# Roda o MESMO cenario 'flight' contra 'model_leak' -- a mesma familia
# de variante de teste do 'plugin-hotswap' (mesmo fonte de producao, atras da
# mesma opcao 'variants'), so que com um UNICO ref() extra em
# BtBehavior::genAction(), nunca balanceado por um unref() (ver o cabecalho
# do script e o #ifdef POC_LEAK_ONE_REF_PER_DECISION em
# models/players/A-4/src/ubf/BtBehavior.cpp). Reproduz de forma PERMANENTE o
# que so tinha sido provado manualmente uma vez (tests/README.md, "ref() a
# mais na FlightAction"). 'model_leak.so' nunca e instalado (so existe em
# models/players/A-4/build/, como as duas variantes de hotswap) -- por isso o
# caminho vem de 'model_build', igual a 'plugin-hotswap' logo abaixo.
test('memory-controle-negativo', py,
     args: files('memory/check_leak_detector_controle_negativo.py') + [
         app.full_path(), model_build / 'libmodel_leak.so'],
     depends: app, workdir: raiz, suite: 'memory',
     timeout: 1800, is_parallel: false)

# ------------------------------------------------------------------------------
# Camada de plugin -- o contrato de carga dinamica
#
# O .so vem de dist/, construido pelo projeto do modelo. Os plugins
# DEFEITUOSOS continuam sendo alvos daqui: sao artefatos de teste, nao modelo.
# ------------------------------------------------------------------------------
bad_plugins = {}
foreach b : ['nosym', 'abi', 'collide', 'collide_own_name']
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
    include_directories : include_directories('../libs'),
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
         '--binario', app.full_path(), '--poc', 'flight',
         '--bad-nosym', bad_plugins['nosym'].full_path(),
         '--bad-abi', bad_plugins['abi'].full_path(),
         '--bad-collide', bad_plugins['collide'].full_path(),
         '--bad-collide-own-name', bad_plugins['collide_own_name'].full_path()],
     depends: [app, bad_plugins['nosym'], bad_plugins['abi'], bad_plugins['collide'],
               bad_plugins['collide_own_name']],
     workdir: raiz, suite: 'plugin', timeout: 900, is_parallel: false)

test('plugin-hotswap', py,
     args: files('plugin/run_plugin_hotswap.py') + [
         '--binario', app.full_path(), '--poc', 'flight',
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
     args: files('plugin/run_unknown_model.py') + [
         '--binario', app.full_path(), '--poc', 'flight',
         '--modelo', dist_plugins / 'libtemplate_mirror.so'],
     depends: app, workdir: raiz, suite: 'plugin',
     timeout: 900, is_parallel: false)

# O DEPOSITO DE TERCEIRO -- prova que plugins/ (o deposito pra um .so
# ja compilado fora deste repositorio, ver plugins/README.md) nao e
# so uma pasta com um README: um .so colocado ali e de fato copiado pra
# dist/lib/mixr-plugins/ (o mesmo passo que o alvo 'models' do Makefile
# raiz faz) e o resultado E USAVEL numa simulacao de verdade -- reusa o
# mirror de contrato do template como o "terceiro" e a MESMA bateria de
# plugin-modelo-estranho.
test('plugin-deposito-terceiro', py,
     args: files('plugin/run_thirdparty_deposit.py') + [
         '--binario', app.full_path(), '--poc', 'flight',
         '--source-so', dist_plugins / 'libtemplate_mirror.so',
         '--run-unknown-model', raiz / 'tests' / 'plugin' / 'run_unknown_model.py'],
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
# MODELO quando ele saiu para models/players/A-4.

# O catalogo de --edl-catalog (tools/extract_execution_chain.py), fonte
# unica de verdade do editor grafico de .edl (src/ui/edl_builder.jsx): tipo/
# unidade/cardinalidade por slot batendo com armadilhas ja confirmadas no
# fonte (ON_SLOT duplicado, slot herdado, ON_SLOT comentado que nao pode
# vazar), sem duplicar nome de fabrica, e cobrindo toda classe usada nos
# cenarios REAIS do repositorio. Puro Python, sem MIXR, sem 'install' --
# suite propria ('tools') porque nao e nem C++ (suite 'domain') nem
# invariante estrutural sobre arquivos do repo (suite 'guard').
# timeout mais folgado que os outros de 'tools' (60s): varre ~600 arquivos
# .cpp/.hpp com regex, ~4-5s sem contencao mas medido estourando 60s sob
# carga alta da maquina (varias suites/sessoes rodando junto) -- nao e um
# hang, e so uma margem apertada demais.
test('edl-catalog', py, args: files('tools/test_edl_catalog.py'),
     workdir: raiz, suite: 'tools', timeout: 120)

# O diagrama de classe UML da aba "Estrutura" de docs/manual/doc.jsx
# (tools/extract_class_diagram.py): atributos/componentes/metodos das 19
# classes fundacionais do MIXR (Referenced, Object, Component, Player,
# Station, ...), extraidos por corpo de classe real -- nao pelas mesmas 3
# macros (DECLARE_SUBCLASS/IMPLEMENT_SUBCLASS/BEGIN_SLOTTABLE) que as outras
# guardas desta suite cobrem. Trava fatos ja conferidos a mao contra o fonte
# real (contagem exata de Referenced, Component->PairStream, os 10 papeis de
# Player via typeid(), NetIO.base sem self-loop) para nao regredir em
# silencio quando um header mudar. Mesma suite 'tools', mesmo motivo de ser
# das guardas acima -- puro Python, sem MIXR, sem 'install'.
test('class-diagram', py, args: files('tools/test_extract_class_diagram.py'),
     workdir: raiz, suite: 'tools', timeout: 120)

# O catalogo da aba "Catalogo" de docs/manual/doc.jsx
# (tools/generate_manual_catalog.py): a soma das 7 factories nativas bate com
# os 225 que models/BUILT-IN.md documenta, 'plugin:A-4' tem exatamente as 9
# classes que o factory.cpp do modelo despacha, nenhum modulo fora de escopo
# aparece, nenhuma classe orfa vaza, e STATS concorda com o proprio MODEL.
# Mesma suite 'tools' e razao de ser da guarda acima -- puro Python, sem
# MIXR, sem 'install'.
test('manual-catalog', py, args: files('tools/test_manual_catalog.py'),
     workdir: raiz, suite: 'tools', timeout: 120)

# src/ui/scripts/edl_lint.py (lint leve de .edl/.edl.in contra o catalogo
# acima) nao pode acusar erro nos cenarios REAIS do repositorio -- ver o
# cabecalho de tools/test_edl_lint.py.
test('edl-lint', py, args: files('tools/test_edl_lint.py'),
     workdir: raiz, suite: 'tools', timeout: 60)

# O ORACULO definitivo (o parser MIXR de verdade, via edlcheck) tem que
# aceitar os MESMOS 12 cenarios reais -- ver o cabecalho de
# tools/test_edlcheck.py. depends:edlcheck garante o binario compilado;
# precisa TAMBEM de 'make install' ja ter rodado (dist/lib/mixr-plugins/
# populado) -- mesma dependencia que toda suite 'scenario'/'plugin' ja tem.
test('edl-check', py,
     args: files('tools/test_edlcheck.py') + ['--binario', edlcheck.full_path()],
     depends: edlcheck, workdir: raiz, suite: 'tools', timeout: 60)

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

# Nenhum PAR de modelos "de verdade" sob models/players/ (fora fixtures/,
# que existem pra IMITAR outro de proposito) pode publicar o MESMO nome
# de fabrica -- ja aconteceu uma vez (ThreadTagProbe, A-4 vs missile) e
# derruba o processo em runtime se algum .edl carregar os dois .so juntos.
# Descobre os modelos por find (mesma filosofia das guardas acima) -- um
# modelo novo ja entra na checagem, sem editar este arquivo.
test('colisao-fabrica', py, args: files('guard/check_colisao_fabrica.py'),
     workdir: raiz, suite: 'guard', timeout: 60)
