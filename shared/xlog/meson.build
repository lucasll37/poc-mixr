# Sistema de log com nivel/stream/arquivo, compartilhado entre os
# subprojetos -- mesmo padrao 'shared/x<nome>' de shared/xtacview/xclock.
#
# Sem factory.cpp: ao contrario de xtacview/xclock/xjoystick, nao ha nada
# aqui para o parser EDL construir -- o mixr::recorder::PrintHandler usado
# como sink e instanciado direto em C++ (ver o comentario em Log.hpp), sem
# passar pelo .epp nem pela cadeia de mixr_factory.cpp.

xlog_inc_dir = include_directories('..')

xlog_lib = static_library('xlog',
    files(
        'Log.cpp',
    ),
    include_directories : xlog_inc_dir,
    dependencies        : [mixr_dep],
)

xlog_dep = declare_dependency(
    link_with           : xlog_lib,
    include_directories : xlog_inc_dir,
    dependencies        : [mixr_dep],
)
