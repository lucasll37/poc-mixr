# ============================================
# Resolve o libdir da dependência mixr via pkg-config
# ============================================
mixr_libdir = mixr_dep.get_variable(pkgconfig: 'libdir')

sources = files(
    'main.cpp',
)

hello = executable('poc-mixr',
    sources,
    include_directories : inc_dir,
    dependencies        : [thread_dep, mixr_dep],
    install             : true,
    install_dir         : get_option('bindir'),
    install_rpath       : mixr_libdir,
    build_rpath         : mixr_libdir,
    link_args           : ['-Wl,--disable-new-dtags'],
)