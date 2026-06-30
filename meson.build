project('poc-mixr', 'cpp',
  version : '0.1.0',
  default_options : [
    'cpp_std=c++11',
    'warning_level=1',
    'buildtype=debugoptimized',
  ]
)

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
