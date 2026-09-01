# O quadro de leitura entre o host e o modelo.
#
# UNICA shared_library() de shared/ -- as outras cinco sao static_library().
# A excecao nao e estilo: este estado e ESCRITO pelo modelo (que mora num .so
# aberto com dlopen) e LIDO pelo executavel. Estatica daria uma copia de cada
# lado, e o dump imprimiria 'bt=--' e 'dec=0' para sempre, sem erro nenhum.
# Ver o cabecalho de Board.hpp.
#
# Instalada, porque o executavel em dist/bin/ precisa acha-la em dist/lib/
# (ver o '$ORIGIN/../lib' no rpath dos alvos que a consomem).

xboard_inc_dir = include_directories('..')

xboard_lib = shared_library('xboard',
    files('Board.cpp'),
    include_directories : xboard_inc_dir,
    install             : true,
    install_dir         : get_option('libdir'),
)

xboard_dep = declare_dependency(
    link_with           : xboard_lib,
    include_directories : xboard_inc_dir,
)
