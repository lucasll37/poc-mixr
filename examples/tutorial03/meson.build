executable(
    'tutorial03',
    files('./main.cpp', './Rng.cpp'),
    dependencies : [
        mixr_base_dep,
        thread_dep
    ],
    link_args : static_flags + ['-lrt'],
    install : true,
    install_rpath : '$ORIGIN/../lib'
)

install_data(
    ['tutorial03.edl'],
    install_dir : get_option('datadir')
)