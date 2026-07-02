executable(
    'tutorial02',
    files('./main.cpp', './Rng.cpp'),
    dependencies : [
        mixr_base_dep,
        thread_dep
    ],
    link_args : [static_flags],
    install : true,
    install_rpath : '$ORIGIN/../lib'
)