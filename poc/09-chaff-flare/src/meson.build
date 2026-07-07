local_inc_dir = include_directories('../include')

sources = files(
    'main.cpp',
    'tacview/RealtimeTelemetryServer.cpp',
)

chaff_flare = executable('chaff-flare',
    sources,
    include_directories : [inc_dir, local_inc_dir],
    dependencies        : [thread_dep, mixr_dep],
    install             : true,
    install_dir         : get_option('bindir'),
    install_rpath       : mixr_libdir,
    build_rpath         : mixr_libdir,
    link_args           : ['-Wl,--disable-new-dtags'],
)
