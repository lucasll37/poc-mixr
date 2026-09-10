# ==============================================================================
# Navstar-3 -- satelite de navegacao (GPS/NAVSTAR) orbitando a Terra, como
# projeto Meson INDEPENDENTE.
#
# Gerado a partir de models/players/template/ por scripts/models.sh
# (make new-model NAME=Navstar-3 CATEGORY=others). Este projeto nao sabe nada
# do host, e o host nao sabe nada dele -- a unica coisa entre os dois e o SDK
# publicado em dist/ pelo projeto principal: o contrato
# (xplugin/PluginAbi.hpp) e as .so de fronteira (libxboard, libxlog,
# libxtrack, ...).
#
# Ver docs/ARCHITECTURE.md para o desenho da orbita/eclipse e o porque de
# NAO haver DynamicsModel nenhum anexado ao player.
# ==============================================================================
project('Navstar-3', 'cpp',
    version: '0.1.0',
    license: 'MIT',
    default_options: [
        'cpp_std=c++17',
        'warning_level=1',
        'werror=false',
    ],
)

cpp = meson.get_compiler('cpp')

# O SDK. Se esta linha falhar, o `make sdk` da raiz nao rodou (ou rodou com
# '--tags sdk' sem o ',devel', que instala as .so e ESQUECE os headers).
sdk_dep = dependency('poc-mixr-sdk', method: 'pkg-config', required: true)

# Do Conan, pelo mesmo .pc que o host usa.
mixr_dep = dependency('mixr', method: 'pkg-config', required: true)

# A arvore de comportamento (BehaviorTree.CPP v3.5.6 -- nada aqui vale para a
# v4, ver contexts/BTCPP-CONTEXT.md).
#
# ARMADILHA JA MEDIDA (nao redescobrir): a lib e' ESTATICA e exporta 447
# simbolos globais 'T'. Sem '-Wl,--exclude-libs,ALL' no link do .so eles
# vazam, porque 'gnu_symbol_visibility: hidden' NAO se aplica a objetos
# vindos de um .a. A flag ja esta em 'model_link_args' abaixo -- nao remova.
behavior_tree_dep = dependency('behaviortree.cpp.asa', method: 'pkg-config', required: true)

mixr_libdir = mixr_dep.get_variable(pkgconfig: 'libdir')
sdk_libdir  = sdk_dep.get_variable(pkgconfig: 'libdir')

# DT_RPATH em vez de DT_RUNPATH -- RPATH e herdado pelas dependencias
# transitivas, RUNPATH nao (ver o Gotcha correspondente no CLAUDE.md raiz).
rpath_link_args = ['-Wl,--disable-new-dtags']

# A impressao digital do build, gravada no descritor e comparada na carga.
# Calculada AQUI, e nao herdada do .pc do SDK, de proposito -- ver o
# comentario identico em models/players/A-4/meson.build para o "porque".
plugin_version_args = [
    '-DMIXR_PLUGIN_PKG_VERSION="' + mixr_dep.version() + '"',
    '-DMIXR_PLUGIN_BUILD_ID="' + cpp.get_id() + ' ' + cpp.version() + '"',
]

model_inc = include_directories('include')

# As duas listas abaixo sao SEPARADAS do resto (e nomeadas) porque tres
# consumidores as reusam sem duplicar caminho nenhum: o .so de producao logo
# abaixo, 'tests/meson.build' (test-domain/test-tree/test-native) e
# 'tools/meson.build' (dump-tree-model). A propriedade que essa separacao
# preserva: as duas compilam SEM o MIXR -- e' o que permite testar a arvore e
# gerar o <TreeNodesModel> sem levantar Station nenhuma.
domain_sources = files(
    # regra de negocio pura (sem MIXR, sem BT.CPP): o propagador orbital
    # circular e a geometria sol/sombra
    'src/domain/CircularOrbit.cpp',
    'src/domain/EclipseGeometry.cpp',
)

bt_sources = files(
    # os nos da arvore e o registro deles (BT.CPP + domain/, sem MIXR)
    'src/bt/bt_factory.cpp',
    'src/bt/nodes/IsEclipsedCondition.cpp',
    'src/bt/nodes/SetSunLabelAction.cpp',
)

model_sources = domain_sources + bt_sources + files(
    'src/plugin.cpp',

    # UBF: percepcao, decisao, atuacao
    'src/ubf/Navstar3State.cpp',
    'src/ubf/Navstar3BtBehavior.cpp',
    'src/ubf/Navstar3Action.cpp',

    # o agente de tempo critico e a fronteira de registro (o dlsym entra
    # por aqui, ver src/plugin.cpp) -- nao ha classe de "corpo fisico"
    # propria: ( SpaceVehicle ) e' construivel direto pela factory nativa
    # mixr::models, ver o comentario no topo de src/xnative/factory.cpp
    'src/xnative/Navstar3AgentTC.cpp',
    'src/xnative/factory.cpp',
)

# shared_module(), NUNCA library() -- um artefato linkavel poderia acabar
# num link_with:, duplicando o MetaObject das suas classes no processo.
#
# gnu_symbol_visibility:'hidden' esconde tudo por padrao; -Wl,--no-undefined
# transforma "chamei algo que o host nao exporta" em erro de LINK, nao de
# dlopen. Nenhum dos dois e opcional -- ver
# models/players/template/docs/CONTRATO.md secao 1.
model_link_args = rpath_link_args + ['-Wl,--no-undefined', '-Wl,--exclude-libs,ALL']
model_deps = [mixr_dep, sdk_dep, behavior_tree_dep]

navstar3_lib = shared_module('Navstar-3',
    model_sources,
    cpp_args              : plugin_version_args,
    link_args             : model_link_args,
    include_directories   : model_inc,
    dependencies          : model_deps,
    gnu_symbol_visibility : 'hidden',
    install               : true,
    install_dir           : get_option('libdir') / 'mixr-plugins',
    install_rpath         : mixr_libdir + ':' + sdk_libdir + ':$ORIGIN/..',
    build_rpath           : mixr_libdir + ':' + sdk_libdir,
)

# A arvore de comportamento vai JUNTO com o .so. Um .so sozinho nao e'
# entrega completa: o slot 'treeFile:' do ( Navstar3BtBehavior ) nomeia este
# XML, e ele e' do MODELO, nao do cenario -- os nos que ele referencia so
# existem dentro desta biblioteca. Mesma decisao (e mesmo destino) de
# models/players/A-4 com flight_tree.xml.
install_data('configs/navstar3_sun_tree.xml',
    install_dir : get_option('datadir') / 'mixr-plugins' / 'Navstar-3')


# Ferramentas de desenvolvimento (dump-tree-model, por tras de
# 'make create-bt'/'make update-bt'). FORA do gate '-Dtests' de proposito:
# nao sao teste.
subdir('tools')

if get_option('tests')
    subdir('tests')
endif
