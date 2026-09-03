# ==============================================================================
# missile -- SEGUNDO modelo, plugin proprio, exemplo de "criar um modelo
# novo" (ver CLAUDE.md e models/README.md secao 2).
#
# Projeto Meson a parte, no molde EXATO de models/fixtures/stub/meson.build:
# so mixr_dep + sdk_dep. NAO linka behaviortree.cpp.asa -- este modelo nao
# decide nada, so voa (guiagem propria em cima do JSBSimModel anexado). E
# carregado ao lado do flight, por um SEGUNDO ( PluginModule ) no cenario de
# demo (src/poc/single-thread/configs/scenario_missile_demo.epp.in), publicando
# so "GuidedMissile" -- os 6 nomes do flight nao mudam, entao 'provides:'
# dos cenarios de PRODUCAO (que so carregam libflight.so) continua igual.
#
# Como todo projeto de modelo deste repositorio, este diretorio tambem tem
# tests/, docs/, Makefile e README.md -- ver o README.md ao lado para o
# build autocontido (dist/ na raiz DESTE projeto).
# ==============================================================================
project('missile', 'cpp',
    version: '1.0.0',
    license: 'MIT',
    default_options: ['cpp_std=c++17', 'warning_level=1', 'werror=false'],
)

cpp = meson.get_compiler('cpp')

sdk_dep  = dependency('poc-mixr-sdk', method: 'pkg-config', required: true)
mixr_dep = dependency('mixr', method: 'pkg-config', required: true)

mixr_libdir = mixr_dep.get_variable(pkgconfig: 'libdir')
sdk_libdir  = sdk_dep.get_variable(pkgconfig: 'libdir')

model_inc = include_directories('src')

missile_lib = shared_module('missile',
    files(
        'src/plugin.cpp',
        'src/domain/Guidance.cpp',
        'src/xmissile/GuidedMissile.cpp',
    ),
    cpp_args : [
        '-DMIXR_PLUGIN_PKG_VERSION="' + mixr_dep.version() + '"',
        '-DMIXR_PLUGIN_BUILD_ID="' + cpp.get_id() + ' ' + cpp.version() + '"',
    ],
    link_args             : ['-Wl,--disable-new-dtags', '-Wl,--no-undefined',
                             '-Wl,--exclude-libs,ALL'],
    include_directories   : model_inc,
    dependencies          : [mixr_dep, sdk_dep],
    gnu_symbol_visibility : 'hidden',
    install               : true,
    install_dir           : get_option('libdir') / 'mixr-plugins',
    install_rpath         : mixr_libdir + ':' + sdk_libdir + ':$ORIGIN/..',
    build_rpath           : mixr_libdir + ':' + sdk_libdir,
)

# Camada 'domain' -- so a lei de guiagem, pura (sem MIXR). Mesmo molde de
# models/flight/tests/meson.build, escalado para um modelo de uma classe so.
if get_option('tests')
    subdir('tests')
endif
