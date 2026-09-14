# ==============================================================================
# Beacon -- modelo de EXERCICIO: um mixr::models::Player minimo que emite e
# trata um evento proprio (events::EID_PING/events::PingMessage, ver
# models/events/README.md e src/plugin.cpp).
#
# Gerado a partir de models/template/ por scripts/models.sh
# (make new-model NAME=Beacon CATEGORY=player), depois REDUZIDO na mao: sem
# domain/bt/ubf/xnative separados (o proposito deste modelo nao e' decisao
# em camadas, e' o mecanismo de EVENTO em si), sem arvore de comportamento,
# sem BehaviorTree.CPP -- so' UMA classe MIXR, direto em src/plugin.cpp (o
# mesmo desenho "1-2 classes -> factory inline" que
# models/template/src/mirror.cpp ja demonstra, aqui para um modelo de
# PRODUCAO de verdade). Projeto Meson INDEPENDENTE: nao sabe nada do core, e
# o core nao sabe nada dele -- a unica coisa entre os dois e' o SDK
# publicado em dist/ pelo projeto principal.
# ==============================================================================
project('Beacon', 'cpp',
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
#
# 'events::PingMessage'/'events::EID_PING' entram por AQUI -- o
# poc-mixr-sdk.pc encadeia '-levents' no proprio 'Libs:' e publica
# dist/include/events/... (ver o meson.build da raiz) -- nenhuma dependencia
# extra precisou ser declarada so' para o payload.
sdk_dep = dependency('poc-mixr-sdk', method: 'pkg-config', required: true)

# Do Conan, pelo mesmo .pc que o core usa. SEM behaviortree.cpp.asa -- este
# modelo nao tem arvore de comportamento nenhuma.
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

# A classe Beacon (include/Beacon.hpp + src/Beacon.cpp), a factory
# (include/xnative/factory.hpp + src/xnative/factory.cpp -- MESMO desenho
# de models/players/A-4, so' com duas classes em vez de dezenas) e a
# fronteira de plugin (src/plugin.cpp). Sem domain/bt/ubf: o proposito
# deste modelo e o mecanismo de EVENTO, nao decisao em camadas.
model_sources = files(
    'src/Beacon.cpp',
    'src/xnative/factory.cpp',
    'src/plugin.cpp',
)

# shared_module(), NUNCA library() -- um artefato linkavel poderia acabar
# num link_with:, duplicando o MetaObject das suas classes no processo.
#
# gnu_symbol_visibility:'hidden' esconde tudo por padrao; -Wl,--no-undefined
# transforma "chamei algo que o core nao exporta" em erro de LINK, nao de
# dlopen. Nenhum dos dois e opcional -- ver models/template/docs/CONTRATO.md
# secao 1. SEM '-Wl,--exclude-libs,ALL': este modelo nao linka nenhuma .a
# estatica (a BehaviorTree.CPP, a unica deste repositorio, nao entra aqui).
model_link_args = rpath_link_args + ['-Wl,--no-undefined']
model_deps = [mixr_dep, sdk_dep]

beacon_lib = shared_module('Beacon',
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

if get_option('tests')
    subdir('tests')
endif
