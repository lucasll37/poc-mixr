executable(
    'tutorial06',
    files(
        './main.cpp',
        'MyComp.cpp'
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
    ['tutorial06a.edl', 'tutorial06b.edl'],
    install_dir : get_option('datadir')
)