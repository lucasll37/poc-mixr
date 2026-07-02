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

mixr_dep = dependency('mixr',
    method:  'pkg-config',
    required: true,
)

# ============================================
# Subdiretórios
# ============================================
subdir('./src')

summary({
    'Build Type':   get_option('buildtype'),
    'C++ Standard': get_option('cpp_std'),
    'Prefix':       get_option('prefix'),
}, section: 'Configuration')

summary({
    'Executable': hello.full_path(),
}, section: 'Build Artifacts')