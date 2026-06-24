# ============================================
# Subdiretórios
# ============================================
subdir('model')



source_files = ['./main.cpp']


executable('PocMixr',
    source_files,
    dependencies:  [mixr_dep],
    install_rpath: '$ORIGIN/../lib',
    install:       true,
    install_dir:   get_option('bindir'),
)