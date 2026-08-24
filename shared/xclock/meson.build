# Controle de velocidade do tempo simulado (acelerar / frear / pausar),
# compartilhado entre os subprojetos -- mesmo padrao 'shared/x<nome>' de
# shared/xtacview (factory propria + classes no namespace mixr::x<nome>).
#
# Sem protobuf aqui: ao contrario do xtacview, esta lib so fala com a
# Station/Simulation nativas.

xclock_inc_dir = include_directories('..')

xclock_lib = static_library('xclock',
    files(
        'ClockStation.cpp',
        'ConsoleKeyboard.cpp',
        'TimeControls.cpp',
        'factory.cpp',
    ),
    include_directories : xclock_inc_dir,
    dependencies        : [mixr_dep],
)

xclock_dep = declare_dependency(
    link_with           : xclock_lib,
    include_directories : xclock_inc_dir,
    dependencies        : [mixr_dep],
)
