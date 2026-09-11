# ==============================================================================
# missile -- exercicio de "como modelar um missil de forma idiomatica em
# MIXR", copiado de models/players/template e personalizado (ver
# docs/PRIMEIROS-PASSOS.md do template para o roteiro; docs/ARCHITECTURE.md
# aqui explica as decisoes especificas deste modelo).
#
# Projeto Meson INDEPENDENTE, pelo mesmo motivo do template de origem: o
# host nao sabe nada deste diretorio, e o `meson setup` dele tem que
# conseguir rodar SEM este fonte existir -- mesma razao documentada em
# models/players/A-4/meson.build.
#
# So' DUAS camadas, nao quatro -- guiagem por navegacao proporcional e
# controle CONTINUO, nao decisao discreta; nem o mixr::models::Missile
# nativo usa arvore de comportamento para isto. bt/ e ubf/ do scaffold de
# origem foram removidos, ver docs/ARCHITECTURE.md.
# ==============================================================================
project('missile', 'cpp',
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

# Do Conan, pelo mesmo .pc que o host usa. NENHUMA dependencia de
# BehaviorTree.CPP aqui -- guiagem por navegacao proporcional e controle
# CONTINUO, nao decisao discreta (ver docs/ARCHITECTURE.md).
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

# 'domain_sources' e' nomeada e SEPARADA do resto porque 'tests/meson.build'
# (test-domain) a reusa sem duplicar caminho nenhum -- a propriedade que
# essa separacao preserva: compila SEM o MIXR, o que faz o teste da regra
# pura rodar em milissegundos, sem levantar Station nenhuma.
domain_sources = files(
    'src/domain/Guidance.cpp',
)

model_sources = domain_sources + files(
    'src/plugin.cpp',
    'src/xnative/GuidedMissile.cpp',

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
model_link_args = rpath_link_args + ['-Wl,--no-undefined']
model_deps = [mixr_dep, sdk_dep]

missile_lib = shared_module('missile',
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
