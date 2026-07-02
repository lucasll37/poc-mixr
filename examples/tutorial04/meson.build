executable(
    'tutorial04',
    files(
        './main.cpp',
        'rngs/AbstractRng.cpp',
        'rngs/Exponential.cpp',
        'rngs/Lognormal.cpp',
        'rngs/Uniform.cpp'
    ),
    dependencies : [
        mixr_base_dep,
        thread_dep
    ],
    link_args : static_flags + ['-lrt'],
    install : true,
    install_rpath : '$ORIGIN/../lib'
)

install_data(
    ['tutorial04.edl'],
    install_dir : get_option('datadir')
)