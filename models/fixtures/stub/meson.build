# ==============================================================================
# stub — UM MODELO ESTRANHO, para verificação
#
# Fica em models/fixtures/ (não como irmão de flight/missile) de propósito:
# não é um modelo de produção, é um FIXTURE de teste -- ver o "porquê" no
# CONTRATO.md e em CLAUDE.md.
#
# Projeto Meson próprio, e isso é o ponto: ele não tem como ver os headers de
# models/flight nem os de src/. As únicas coisas no include path dele são
# o SDK publicado em dist/ e o MIXR.
#
# Ele NÃO linka a BehaviorTree.CPP — um modelo não precisa decidir com árvore.
# Isso também é parte do que se verifica: o contrato não impõe a pilha interna.
#
# Ver models/fixtures/stub/docs/CONTRATO.md para as obrigações que este arquivo
# existe para exercitar.
#
# Como todo projeto de modelo deste repositório, este diretório também tem
# tests/, docs/, Makefile e README.md -- ver o README.md ao lado para o build
# autocontido (dist/ na raiz DESTE projeto, sem depender do Makefile raiz).
# ==============================================================================
project('stub', 'cpp',
    version: '1.0.0',
    license: 'MIT',
    default_options: ['cpp_std=c++17', 'warning_level=1', 'werror=false'],
)

cpp = meson.get_compiler('cpp')

sdk_dep  = dependency('poc-mixr-sdk', method: 'pkg-config', required: true)
mixr_dep = dependency('mixr', method: 'pkg-config', required: true)

mixr_libdir = mixr_dep.get_variable(pkgconfig: 'libdir')
sdk_libdir  = sdk_dep.get_variable(pkgconfig: 'libdir')

stub_lib = shared_module('stub',
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

# A suite de CONTRATO -- nao a suite de comportamento do host (essa e
# tests/plugin/run_stub_model.py, na raiz, e continua rodando so a partir do
# Makefile raiz). Esta e local a este projeto: os dois criterios "medidos"
# que models/README.md §2.3 ja documentava a mao (1 simbolo T exportado, zero
# dependencia "not found") viram um `meson test` de verdade, sem precisar de
# gtest -- e por isso o default de -Dtests aqui e so um script, nao uma
# suite C++.
if get_option('tests')
    subdir('tests')
endif
