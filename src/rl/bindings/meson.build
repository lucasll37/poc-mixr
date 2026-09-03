# O modulo de extensao Python (_native) -- ver rl/README.md.
#
# So este alvo linka pybind11/o Python de desenvolvimento -- declarado AQUI,
# nao no meson.build raiz, porque e o UNICO consumidor (mesmo padrao do
# ftxui_dep em app/src/meson.build).
python3_mod = import('python')
py3 = python3_mod.find_installation('python3', required: true)
py3_dep = py3.dependency(embed: false)

pybind11_dep = dependency('pybind11', method: 'pkg-config', required: true)

# '$ORIGIN/../../lib': a extensao instala em dist/python/mixr_gym/, DOIS
# niveis abaixo de dist/ -- diferente de dist/bin/ (um nivel), onde
# own_libs_rpath ($ORIGIN/../lib) das outras pocs basta.
rl_own_libs_rpath = '$ORIGIN/../../lib'

native_sources = files(
    'mixr_factory.cpp',
    'StationBuilder.cpp',
    'NativeSimulation.cpp',
    'PyBindings.cpp',
)

py3.extension_module('_native',
    native_sources,
    cpp_args             : asan_cpp_args,
    link_args            : asan_link_args + rpath_link_args,
    include_directories  : include_directories('.'),
    dependencies         : [thread_dep, mixr_dep, xplugin_dep, xtacview_dep,
                            xrlbridge_dep, pybind11_dep, py3_dep],
    install              : true,
    install_dir          : rl_python_pkg_dir,
    install_rpath        : mixr_libdir + ':' + rl_own_libs_rpath,
    build_rpath          : mixr_libdir,
)
