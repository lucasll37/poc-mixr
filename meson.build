project('mixr-hello', 'cpp',
    version: '1.0.0',
    license: 'MIT',
    default_options: [
        'cpp_std=c++17',
        'warning_level=1',
        'werror=false',
    ],
)

message('mixr-hello - Meson Build')
message('Version: ' + meson.project_version())
message('Build Type: ' + get_option('buildtype'))

# ============================================
# Dependências
# ============================================
thread_dep = dependency('threads')

mixr_dep = dependency('mixr', method: 'pkg-config', required: true)
behavior_tree_dep = dependency('behaviortree.cpp.asa', method: 'pkg-config', required: true)

# Resolve o libdir da dependência mixr via pkg-config (compartilhado
# por todos os executáveis: raiz e subprojetos em poc/)
mixr_libdir = mixr_dep.get_variable(pkgconfig: 'libdir')

# ============================================
# Includes
# ============================================
inc_dir = include_directories('include')

# ============================================
# Subdiretórios
# ============================================
subdir('./src')

# Subprojetos incrementais da PoC (cada um coexiste como um executável
# próprio). Adicionar um novo subdir() aqui para cada poc/NN-slug novo.
subdir('./poc/01-flying-aircraft')

# ============================================
# Summary
# ============================================
summary({
    'Build Type':   get_option('buildtype'),
    'C++ Standard': get_option('cpp_std'),
    'Prefix':       get_option('prefix'),
}, section: 'Configuration')

summary({
    'poc-mixr (hello)':        hello.full_path(),
    'poc/01-flying-aircraft':  flying_aircraft.full_path(),
}, section: 'Build Artifacts')