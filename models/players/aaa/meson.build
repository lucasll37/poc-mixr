# ==============================================================================
# aaa -- uma antiaerea (bateria de superficie-ar) estacionaria, com "domo" de
# alcance, que dispara mixr::models::xmissile::GuidedMissile
# (models/players/missile) contra qualquer alvo hostil que entre no alcance.
#
# Exercicio (ver TODO.md, raiz): "implementar uma antiaerea com domo que
# dispara o mesmo missil (ja implementado num A-4) que entra no seu raio de
# atuacao" -- verificar como isso e' modelado de forma idiomatica no MIXR,
# nao um sistema de armas de producao.
#
# Projeto Meson INDEPENDENTE, copiado de models/template/ (ver
# docs/PRIMEIROS-PASSOS.md la'): nao sabe nada do host, e o host nao sabe
# nada dele -- a unica coisa entre os dois e' o SDK publicado em dist/ pelo
# projeto principal (xplugin/PluginAbi.hpp + as .so de fronteira libxboard/
# libxlog/libxtrack/...). E' POR ISSO que e' um project() proprio e nao um
# subdir(): o `meson setup` do host tem que conseguir rodar SEM o fonte
# daqui existir.
#
# Ao contrario de models/players/missile (guiagem = controle continuo, sem
# arvore de comportamento), esta antiaerea usa a pilha COMPLETA
# domain/bt/ubf/xnative -- decisao escolhida deliberadamente com o usuario
# (AskUserQuestion), para espelhar a arquitetura do A-4 mesmo sendo uma
# regra unica ("alvo no domo + arma disponivel -> dispara"). Ver
# docs/ARCHITECTURE.md para o "porque" completo, inclusive por que NAO ha
# nenhuma subclasse de AgentTC aqui (( UbfAgent ) nativo basta).
#
# Este diretorio nao hospeda mirror de contrato nenhum (ao contrario de
# models/template/, que hospeda DOIS artefatos) -- 'src/mirror.cpp' NAO faz
# parte do scaffold copiavel e ja foi removido por 'make new-model'.
# ==============================================================================
project('aaa', 'cpp',
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
# abaixo, 'tests/meson.build' (test-domain/test-tree) e 'tools/meson.build'
# (dump-tree-model). A propriedade que essa separacao preserva: as duas
# compilam SEM o MIXR -- e' o que permite testar a arvore e gerar o
# <TreeNodesModel> sem levantar Station nenhuma.
domain_sources = files(
    # regra de negocio pura (sem MIXR, sem BT.CPP): o domo de alcance
    'src/domain/DomePolicy.cpp',
)

bt_sources = files(
    # os nos da arvore e o registro deles (BT.CPP + domain/, sem MIXR)
    'src/bt/bt_factory.cpp',
    'src/bt/nodes/TargetInDomeCondition.cpp',
    'src/bt/nodes/FireMissileAction.cpp',
    'src/bt/nodes/WatchAction.cpp',
)

model_sources = domain_sources + bt_sources + files(
    'src/plugin.cpp',

    # UBF: percepcao, decisao, atuacao
    'src/ubf/AaaState.cpp',
    'src/ubf/AaaBehavior.cpp',
    'src/ubf/AaaAction.cpp',

    # o Player estacionario em si (subclasse de mixr::models::SamVehicle)
    'src/xnative/AaaSite.cpp',

    # fronteira de registro (o dlsym entra por aqui, ver src/plugin.cpp)
    'src/xnative/factory.cpp',
)

# shared_module(), NUNCA library() -- um artefato linkavel poderia acabar
# num link_with:, duplicando o MetaObject das suas classes no processo.
#
# gnu_symbol_visibility:'hidden' esconde tudo por padrao; -Wl,--no-undefined
# transforma "chamei algo que o host nao exporta" em erro de LINK, nao de
# dlopen. Nenhum dos dois e opcional -- ver
# models/template/docs/CONTRATO.md secao 1.
model_link_args = rpath_link_args + ['-Wl,--no-undefined', '-Wl,--exclude-libs,ALL']
model_deps = [mixr_dep, sdk_dep, behavior_tree_dep]

aaa_lib = shared_module('aaa',
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
# entrega completa: o slot 'treeFile:' do ( AaaBehavior ) nomeia este XML, e
# ele e' do MODELO, nao do cenario -- os nos que ele referencia so' existem
# dentro desta biblioteca. Mesma decisao (e mesmo destino) de
# models/players/A-4 com flight_tree.xml e models/players/missile com... na
# verdade o missile nao tem arvore (guiagem e' controle continuo); o molde
# mais direto aqui e' o proprio A-4.
install_data('configs/aaa_tree.xml',
    install_dir : get_option('datadir') / 'mixr-plugins' / 'aaa')


# Ferramentas de desenvolvimento (dump-tree-model, por tras de
# 'make create-bt'/'make update-bt'). FORA do gate '-Dtests' de proposito:
# nao sao teste.
subdir('tools')

if get_option('tests')
    subdir('tests')
endif
