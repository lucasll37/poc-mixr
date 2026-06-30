project('poc-mixr', 'cpp',
  version : '0.1.0',
  default_options : [
      'default_library=shared',
      'c_std=c11',
      'cpp_std=c++17',
      'c_args=-fPIC',
      'cpp_args=-fPIC',
  ]
)


#
# Linker flags for mostly static builds
#

static_flags = [
    '-static-libstdc++',   # embed libstdc++.a
    '-static-libgcc',      # embed libgcc.a
]

#
# Dependencies
#

thread_dep  = dependency('threads')
jsbsim_dep  = dependency('jsbsim', required : false)

# ── MIXR ─────────────────────────────────────────────────────────────────────
# Resolvido via Conan (PkgConfigDeps), procurando o .pc 'mixr' gerado pelo
# `conan install` em build/. Garanta que pkg_config_path aponte para lá
# (ver Makefile: -Dpkg_config_path=...).
mixr_dep = dependency('mixr')

poc_deps = declare_dependency(
  include_directories : include_directories('include'),
  dependencies : [mixr_dep, thread_dep, jsbsim_dep],
)

subdir('src')
