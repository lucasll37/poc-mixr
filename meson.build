project('poc-mixr', 'cpp',
    version: '0.0.1',
    default_options: [
        'c_std=c11',
        'cpp_std=c++17',
        'warning_level=3',
        'werror=false',
    ],
)

message('Version: ' + meson.project_version())
message('Build Type: ' + get_option('buildtype'))

cpp = meson.get_compiler('cpp')

add_project_arguments(
    cpp.get_supported_arguments(['-Wno-pedantic', '-Wno-unused-parameter']),
    language: 'cpp',
)

add_project_link_arguments(
    '-Wl,-z,noexecstack',
    language: 'cpp',
)

# ============================================
# Dependências
# ============================================
mixr_dep = dependency('mixr/1.0.5')

# ============================================
# Subdiretórios
# ============================================
subdir('src')
