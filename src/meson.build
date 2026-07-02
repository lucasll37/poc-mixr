sources = files(
    'main.cpp',
)

hello = executable('poc-mixr',
    sources,
    include_directories : inc_dir,
    dependencies        : [thread_dep, mixr_dep],
    install             : true,
    install_dir         : get_option('bindir'),
    install_rpath       : '$ORIGIN/../lib',
    build_rpath         : '$ORIGIN/../lib',
)