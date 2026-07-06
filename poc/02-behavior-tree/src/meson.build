sources = files(
    'main.cpp',
)

behavior_tree_poc = executable('behavior-tree',
    sources,
    include_directories : inc_dir,
    dependencies        : [thread_dep, behavior_tree_dep],
    install             : true,
    install_dir         : get_option('bindir'),
)
