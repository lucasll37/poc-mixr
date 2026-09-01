# Sistema de log com nivel/stream/arquivo, compartilhado entre os
# subprojetos -- mesmo padrao 'shared/x<nome>' de shared/xtacview/xclock.
#
# shared_library() e nao static_library(), ao contrario das outras libs de
# shared/. O motivo e de CORRECAO, nao de conveniencia: o modelo mora num .so
# carregado com dlopen e usa LOG(...) ao carregar a arvore de comportamento.
# Com duas copias, o setLoggingEnabled(false) que o main.cpp chama sob
# '-deterministic' nao alcancaria o lado do plugin, e o modo comparavel
# passaria a emitir linhas com timestamp de parede -- quebrando justamente o
# que o modo existe para garantir.
#
# Sem factory.cpp: ao contrario de xtacview/xclock/xjoystick, nao ha nada
# aqui para o parser EDL construir -- o mixr::recorder::PrintHandler usado
# como sink e instanciado direto em C++ (ver o comentario em Log.hpp), sem
# passar pelo .epp nem pela cadeia de mixr_factory.cpp.

xlog_inc_dir = include_directories('..')

xlog_lib = shared_library('xlog',
    files(
        'Log.cpp',
    ),
    include_directories : xlog_inc_dir,
    dependencies        : [mixr_dep],
    install             : true,
    install_dir         : get_option('libdir'),
    install_tag         : 'sdk',
    build_rpath         : mixr_libdir,
    install_rpath       : mixr_libdir,
)

xlog_dep = declare_dependency(
    link_with           : xlog_lib,
    include_directories : xlog_inc_dir,
    dependencies        : [mixr_dep],
)
