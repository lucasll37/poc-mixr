# Runner headless e independente de UM cenario passado por argumento --
# peer enxuto de ./app, mas SEM nenhuma dependencia do codigo de app/:
# fabrica, construcao de Station e encerramento proprios, escritos do zero
# seguindo a MESMA receita que src/rl/bindings/ ja demonstra para um host
# independente (ver README.md desta pasta para o "porque").
node_sources = files('main.cpp', 'Run.cpp', 'factory.cpp')

node = executable('node',
    node_sources,
    include_directories : include_directories('.'),
    dependencies        : [thread_dep, mixr_dep, xtacview_dep, xclock_dep,
                           xjoystick_dep, xmsg_dep, xplugin_dep, xlog_dep],
    link_args           : rpath_link_args,
    install             : true,
    install_dir         : get_option('bindir'),
    install_rpath       : mixr_libdir + ':' + own_libs_rpath,
    build_rpath         : mixr_libdir,
)
