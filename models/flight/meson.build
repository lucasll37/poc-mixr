# ==============================================================================
# flight — o MODELO de producao, como projeto Meson INDEPENDENTE
#
# Este projeto nao sabe nada do host, e o host nao sabe nada dele. A unica
# coisa entre os dois e o SDK publicado em dist/ pelo projeto principal:
# o contrato (xplugin/PluginAbi.hpp) e as tres .so de fronteira
# (libxboard, libxlog, libxtrack).
#
# E POR ISSO que ele e um project() proprio e nao um subdir: o `meson setup`
# do host tem de conseguir rodar SEM os fontes daqui existirem. Enquanto o
# modelo era um alvo do host, o files() dele listava os 24 .cpp e o build do
# host exigia o fonte -- o oposto do cenario que se quer verificar.
#
# ORDEM (ver o Makefile):  deps -> sdk -> models -> build
#
# UMA arvore, DOIS artefatos: libflight.so e libflight_tc.so, o segundo com
# -DFLIGHT_TC_AGENT (o agente de tempo critico). Era a unica diferenca entre
# as duas copias que existiam antes.
# ==============================================================================
project('flight', 'cpp',
    version: '1.0.0',
    license: 'MIT',
    default_options: [
        'cpp_std=c++17',
        'warning_level=1',
        'werror=false',
    ],
)

cpp = meson.get_compiler('cpp')

# O SDK. Se esta linha falhar, o `make sdk` nao rodou (ou rodou com
# '--tags sdk' sem o ',devel', que instala as .so e ESQUECE os headers).
sdk_dep = dependency('poc-mixr-sdk', method: 'pkg-config', required: true)

# Do Conan, pelos mesmos .pc que o host usa. O modelo linka a
# BehaviorTree.CPP; o host NAO (ela e estatica -- linkada dos dois lados,
# duplicaria os estaticos dela no processo).
mixr_dep = dependency('mixr', method: 'pkg-config', required: true)
behavior_tree_dep = dependency('behaviortree.cpp.asa', method: 'pkg-config', required: true)

mixr_libdir = mixr_dep.get_variable(pkgconfig: 'libdir')
sdk_libdir  = sdk_dep.get_variable(pkgconfig: 'libdir')

# DT_RPATH em vez de DT_RUNPATH -- RPATH e herdado pelas dependencias
# transitivas, RUNPATH nao.
rpath_link_args = ['-Wl,--disable-new-dtags']

asan_cpp_args  = get_option('asan') ? ['-fsanitize=address', '-fno-omit-frame-pointer', '-g'] : []
asan_link_args = get_option('asan') ? ['-fsanitize=address'] : []

# A impressao digital do build, gravada no descritor e comparada na carga.
#
# Calculada AQUI, e nao herdada do .pc do SDK, de proposito: o campo descreve
# quem construiu O PLUGIN. Se viesse do SDK, os dois lados reportariam o mesmo
# numero por construcao e a checagem viraria tautologia. (Tecnicamente
# funcionaria: medido que as aspas escapadas atravessam o .pc intactas.)
plugin_version_args = [
    '-DMIXR_PLUGIN_PKG_VERSION="' + mixr_dep.version() + '"',
    '-DMIXR_PLUGIN_BUILD_ID="' + cpp.get_id() + ' ' + cpp.version() + '"',
]

model_inc = include_directories('include')

model_sources = files(
    'src/plugin.cpp',

    # regras de negocio puras (sem MIXR, sem BehaviorTree.CPP)
    'src/domain/geometry.cpp',
    'src/domain/LaunchPolicy.cpp',
    'src/domain/PatrolPlan.cpp',
    'src/domain/RtbPlan.cpp',
    'src/domain/TerrainFloor.cpp',
    'src/domain/ThreatPolicy.cpp',

    # classes MIXR proprias
    'src/xnative/AlertDatalink.cpp',
    'src/xnative/factory.cpp',
    'src/xnative/ThreadTag.cpp',
    'src/xnative/RadarScan.cpp',
    'src/xnative/FlightAgentTC.cpp',

    # UBF: percepcao, decisao (arvore dentro), atuacao
    'src/ubf/FlightState.cpp',
    'src/ubf/FlightAction.cpp',
    'src/ubf/BtBehavior.cpp',
    'src/ubf/BtBehaviorSlots.cpp',
    'src/ubf/AltitudeSafetyBehavior.cpp',
    'src/ubf/RLBridgeBehavior.cpp',

    # nos/factory da arvore de comportamento
    'src/bt/bt_factory.cpp',
    'src/bt/bt_factory_sdk.cpp',
    'src/bt/nodes/AlertReceivedCondition.cpp',
    'src/bt/nodes/ContactDetectedCondition.cpp',
    'src/bt/nodes/FuelLowCondition.cpp',
    'src/bt/nodes/LaunchEnvelopeCondition.cpp',
    'src/bt/nodes/LaunchMissileAction.cpp',
    'src/bt/nodes/PatrolAction.cpp',
    'src/bt/nodes/OnnxScoreCondition.cpp',
    'src/bt/nodes/OnnxPolicyAction.cpp',
    'src/bt/nodes/PyDecideAction.cpp',
    'src/bt/nodes/ReportAndEvadeAction.cpp',
    'src/bt/nodes/ReturnToBaseAction.cpp',
    'src/bt/nodes/SupportAlertAction.cpp',
)

# -Wl,--exclude-libs,ALL e OBRIGATORIO: a BehaviorTree.CPP e um .a com 447
# simbolos T globais, e gnu_symbol_visibility:'hidden' NAO se aplica a objetos
# vindos de arquivo estatico. Sem a flag eles vazariam para o .dynsym e a
# guarda de simbolo do host ficaria vermelha -- com razao.
model_link_args = asan_link_args + rpath_link_args + ['-Wl,--no-undefined', '-Wl,--exclude-libs,ALL']
model_deps = [mixr_dep, behavior_tree_dep, sdk_dep]

# '$ORIGIN/..' porque o plugin instala em dist/lib/mixr-plugins/ e as .so do
# SDK ficam em dist/lib/.
model_rpath = mixr_libdir + ':' + sdk_libdir + ':$ORIGIN/..'

foreach v : [['flight', []], ['flight_tc', ['-DFLIGHT_TC_AGENT']]]
    shared_module(v[0],
        model_sources,
        cpp_args              : asan_cpp_args + plugin_version_args + v[1],
        link_args             : model_link_args,
        include_directories   : model_inc,
        dependencies          : model_deps,
        gnu_symbol_visibility : 'hidden',
        install               : true,
        install_dir           : get_option('libdir') / 'mixr-plugins',
        install_rpath         : model_rpath,
        build_rpath           : mixr_libdir + ':' + sdk_libdir,
    )
endforeach

# A arvore de comportamento vai JUNTO. Um .so sozinho nao e entrega completa:
# o slot 'treeFile:' do ( BtBehavior ) nomeia este XML, e ele e do modelo, nao
# do cenario.
install_data('configs/flight_tree.xml',
    install_dir : get_option('datadir') / 'mixr-plugins' / 'flight')

# A arvore de DEPLOY da politica de RL e o .onnx que ela aponta. Vao junto
# pelo mesmo motivo do flight_tree.xml: sao do MODELO, nao do cenario -- a
# politica foi treinada contra ESTA observacao e ESTA aeronave.
#
# O .onnx de exemplo tem pesos ALEATORIOS, nao treinados (ver
# src/poc/rl-training/tools/export_onnx.py --random). Ele existe para exercitar a cadeia
# inteira -- carga, forma, inferencia, desnormalizacao, atuacao -- sem
# depender de uma sessao de treino. Uma politica de verdade substitui o
# arquivo, sem recompilar nada.
install_data('configs/flight_tree_onnx.xml',
    install_dir : get_option('datadir') / 'mixr-plugins' / 'flight')
install_data('configs/policy_example.onnx',
    install_dir : get_option('datadir') / 'mixr-plugins' / 'flight')

# A arvore de PROTOTIPAGEM e o script que ela aponta -- ver
# shared/xpyembed/PyEmbed.hpp.
install_data('configs/flight_tree_py.xml',
    install_dir : get_option('datadir') / 'mixr-plugins' / 'flight')
install_data('configs/policy_example.py',
    install_dir : get_option('datadir') / 'mixr-plugins' / 'flight')

# A aeronave TAMBEM vai junto, pelo mesmo motivo: o modelo (domain/ThreatPolicy,
# BtTuning) e calibrado para o c310 especificamente -- maxClimbRateMps, a folga
# do piso anti-CFIT, os limiares de combustivel -- entao a definicao JSBSim dela
# nao e dado de CENARIO (que varia por poc), e sim do modelo (que nao varia). As
# tres pocs vendorizavam a mesma copia byte-identica antes disto; agora e uma
# so, instalada por aqui e referenciada por 'rootDir:' em todo cenario que usa
# ( JSBSimModel model: "c310" ) -- inclusive src/poc/dis/bandit, que nao carrega o
# plugin mas pilota a mesma aeronave.
install_subdir('data/jsbsim',
    install_dir : get_option('datadir') / 'mixr-plugins' / 'flight')

# As duas variantes de hot-swap, do MESMO fonte, diferindo so no sentido da
# curva do circuito de patrulha. Consumidas pelo teste 'plugin-hotswap' do
# host. Atras de uma opcao para nao poluir um build comum.
if get_option('variants')
    foreach v : [['model_variant_a', '1.0'], ['model_variant_b', '-1.0']]
        shared_module(v[0],
            model_sources,
            cpp_args              : plugin_version_args + ['-DPOC_MODEL_TURN_SIGN=' + v[1]],
            link_args             : rpath_link_args + ['-Wl,--no-undefined', '-Wl,--exclude-libs,ALL'],
            include_directories   : model_inc,
            dependencies          : model_deps,
            gnu_symbol_visibility : 'hidden',
            build_rpath           : mixr_libdir + ':' + sdk_libdir,
            install               : false,
        )
    endforeach
endif

if get_option('tests')
    subdir('tests')
endif
