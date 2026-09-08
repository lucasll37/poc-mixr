# ==============================================================================
# template -- o PONTO DE PARTIDA para um modelo novo, como projeto Meson
# INDEPENDENTE.
#
# Este projeto nao sabe nada do host, e o host nao sabe nada dele -- a unica
# coisa entre os dois e o SDK publicado em dist/ pelo projeto principal: o
# contrato (xplugin/PluginAbi.hpp) e as .so de fronteira (libxboard, libxlog,
# libxtrack, ...). E POR ISSO que ele e um project() proprio e nao um
# subdir(): o `meson setup` do host tem que conseguir rodar SEM os fontes
# daqui existirem -- mesma razao documentada em models/players/A-4/meson.build.
#
# NAO e um projeto de PRODUCAO -- ele nunca entra no `models:` do Makefile
# raiz por MODELOS_PRODUCAO (ver ../../../Makefile, que o instala a parte)
# nem em `tests/meson.build` da raiz. Compila e testa sozinho. O ARTEFATO
# 'template' (template_lib, logo abaixo) e o unico proposito de existir
# deste diretorio como PONTO DE PARTIDA: ser COPIADO e RENOMEADO -- ver
# docs/PRIMEIROS-PASSOS.md.
#
# Este diretorio hospeda, de proposito, DOIS artefatos com papeis
# diferentes -- ver a diferenca detalhada em docs/ARCHITECTURE.md:
#   * 'template' (template_lib) -- o scaffold copiavel de verdade: mostra a
#     separacao em camadas (domain/ubf/xnative) que os modelos de producao
#     usam, com uma unica decisao de exemplo em cada camada. Nao serve
#     nenhum cenario existente, porque nao deveria.
#   * 'template_mirror' (template_mirror_lib, definido mais abaixo) -- o
#     mirror de contrato que este repositorio usa nos proprios testes de
#     plugin (herdou o papel de models/players/fixtures/stub, removido). NAO
#     faz parte do scaffold -- apagar antes de personalizar uma copia.
# ==============================================================================
project('template', 'cpp',
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

# Do Conan, pelo mesmo .pc que o host usa. Este template NAO linka
# behaviortree.cpp.asa -- decide com uma regra so (ver
# include/ubf/ExampleBehavior.hpp). Acrescente 'behavior_tree_dep =
# dependency(...)' e ponha-a em model_deps quando o SEU modelo crescer alem
# disso (ver docs/ARCHITECTURE.md, secao "Quando isto nao bastar mais").
mixr_dep = dependency('mixr', method: 'pkg-config', required: true)

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

model_sources = files(
    'src/plugin.cpp',

    # regra de negocio pura (sem MIXR) -- acrescente a sua ao lado desta
    'src/domain/ExampleThreshold.cpp',

    # UBF: percepcao, decisao, atuacao
    'src/ubf/ExampleState.cpp',
    'src/ubf/ExampleBehavior.cpp',
    'src/ubf/ExampleAction.cpp',

    # fronteira de registro (o dlsym entra por aqui, ver src/plugin.cpp)
    'src/xnative/factory.cpp',
)

# shared_module(), NUNCA library() -- um artefato linkavel poderia acabar
# num link_with:, duplicando o MetaObject das suas classes no processo.
#
# gnu_symbol_visibility:'hidden' esconde tudo por padrao; -Wl,--no-undefined
# transforma "chamei algo que o host nao exporta" em erro de LINK, nao de
# dlopen. Nenhum dos dois e opcional -- ver
# models/players/fixtures/stub/docs/CONTRATO.md secao 1.
model_link_args = rpath_link_args + ['-Wl,--no-undefined', '-Wl,--exclude-libs,ALL']
model_deps = [mixr_dep, sdk_dep]

template_lib = shared_module('template',
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

# >>> MIRROR-BLOCK-START -- scripts/models.sh apaga tudo ate MIRROR-BLOCK-END
# quando copia este diretorio como scaffold. Nao mova/reformate as duas
# linhas de marcador.
# ==========================================================================
# SEGUNDO artefato deste projeto: NAO faz parte do scaffold copiavel (ver o
# aviso no topo de src/mirror.cpp e em docs/PRIMEIROS-PASSOS.md). Existe so
# para este repositorio provar, nos seus proprios testes de plugin
# (tests/plugin/run_unknown_model.py, tests/meson.build raiz), que "o
# contrato de plugin basta" contra o cenario de PRODUCAO deste repo --
# herdou esse papel de models/players/fixtures/stub (removido).
#
# Exporta os MESMOS 9 nomes de fabrica que models/players/A-4 exporta, de
# proposito -- e o que 'plugin-modelo-estranho'/'plugin-deposito-terceiro'
# precisam para trocar so o 'file:' do cenario de producao e ver a
# aplicacao inteira subir com um modelo que nunca viu.
# ==========================================================================
template_mirror_lib = shared_module('template_mirror',
    files('src/mirror.cpp'),
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
# <<< MIRROR-BLOCK-END

if get_option('tests')
    subdir('tests')
endif
