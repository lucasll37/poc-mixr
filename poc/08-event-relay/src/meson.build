local_inc_dir = include_directories('../include')

sources = files(
    'main.cpp',
    'mixr_factory.cpp',
    'events.cpp',
    'RadarContactRelay.cpp',
    'AlertReceiver.cpp',
)

event_relay = executable('event-relay',
    sources,
    include_directories : [inc_dir, local_inc_dir],
    dependencies        : [thread_dep, mixr_dep, xtacview_dep],
    install             : true,
    install_dir         : get_option('bindir'),
    install_rpath       : mixr_libdir,
    build_rpath         : mixr_libdir,
    link_args           : rpath_link_args,
)
