sources = files(
    'main.cpp',
)

radar_detection = executable('radar-detection',
    sources,
    include_directories : inc_dir,
    dependencies        : [thread_dep, mixr_dep],
    install             : true,
    install_dir         : get_option('bindir'),
    install_rpath       : mixr_libdir,
    build_rpath         : mixr_libdir,
    link_args           : ['-Wl,--disable-new-dtags'],
)
