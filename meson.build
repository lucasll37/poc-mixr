project('poc-mixr', 'cpp',
  version : '0.1.0',
  default_options : [
    'cpp_std=c++11',
    'warning_level=1',
    'buildtype=debugoptimized',
  ]
)

cpp = meson.get_compiler('cpp')

thread_dep  = dependency('threads')
jsbsim_dep  = dependency('JSBSim', required : false)

# ── MIXR ─────────────────────────────────────────────────────────────────────
mixr_prefix = get_option('mixr_prefix')
mixr_inc    = include_directories(mixr_prefix / 'include')

mixr_base_lib    = cpp.find_library('mixr_base',       dirs : mixr_prefix / 'lib', required : true)
mixr_sim_lib     = cpp.find_library('mixr_simulation',  dirs : mixr_prefix / 'lib', required : true)
mixr_models_lib  = cpp.find_library('mixr_models',      dirs : mixr_prefix / 'lib', required : true)

mixr_dep = declare_dependency(
  include_directories : [mixr_inc, include_directories('include')],
  dependencies : [mixr_base_lib, mixr_sim_lib, mixr_models_lib,
                  thread_dep, jsbsim_dep],
)

subdir('src')
