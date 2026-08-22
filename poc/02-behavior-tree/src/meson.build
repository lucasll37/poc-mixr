sources = files(
    'main.cpp',
)

behavior_tree_poc = executable('behavior-tree',
    sources,
    include_directories : inc_dir,
    dependencies        : [thread_dep, behavior_tree_dep],
    install             : true,
    install_dir         : get_option('bindir'),
    install_rpath       : bt_libdir,
    build_rpath         : bt_libdir,
    link_args           : rpath_link_args,
)
