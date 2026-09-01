# ==============================================================================
# stub-model — UM MODELO ESTRANHO, para verificação
#
# Projeto Meson próprio, e isso é o ponto: ele não tem como ver os headers de
# models/flight-model nem os de src/. As únicas coisas no include path dele são
# o SDK publicado em dist/ e o MIXR.
#
# Ele NÃO linka a BehaviorTree.CPP — um modelo não precisa decidir com árvore.
# Isso também é parte do que se verifica: o contrato não impõe a pilha interna.
#
# Ver models/stub-model/CONTRATO.md para as obrigações que este arquivo existe
# para exercitar.
# ==============================================================================
project('stub-model', 'cpp',
    version: '1.0.0',
    license: 'MIT',
    default_options: ['cpp_std=c++17', 'warning_level=1', 'werror=false'],
)

cpp = meson.get_compiler('cpp')

sdk_dep  = dependency('poc-mixr-sdk', method: 'pkg-config', required: true)
mixr_dep = dependency('mixr', method: 'pkg-config', required: true)

mixr_libdir = mixr_dep.get_variable(pkgconfig: 'libdir')
sdk_libdir  = sdk_dep.get_variable(pkgconfig: 'libdir')

shared_module('stub_model',
    files('src/stub.cpp'),
    cpp_args : [
        '-DMIXR_PLUGIN_PKG_VERSION="' + mixr_dep.version() + '"',
        '-DMIXR_PLUGIN_BUILD_ID="' + cpp.get_id() + ' ' + cpp.version() + '"',
    ],
    link_args             : ['-Wl,--disable-new-dtags', '-Wl,--no-undefined',
                             '-Wl,--exclude-libs,ALL'],
    dependencies          : [mixr_dep, sdk_dep],
    gnu_symbol_visibility : 'hidden',
    install               : true,
    install_dir           : get_option('libdir') / 'mixr-plugins',
    install_rpath         : mixr_libdir + ':' + sdk_libdir + ':$ORIGIN/..',
    build_rpath           : mixr_libdir + ':' + sdk_libdir,
)
