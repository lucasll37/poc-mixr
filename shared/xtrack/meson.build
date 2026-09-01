# Consulta ao radar NATIVO -- qual e o contato hostil mais proximo.
#
# Promovida a shared_library() e a PARTE DO SDK porque e a unica peca disputada
# pelos dois lados da fronteira: o HOST precisa dela para o 'track='/'trackRange='
# do dump, e o MODELO precisa dela para a percepcao (ubf/FlightState).
#
# A alternativa seria compilar o mesmo .cpp dos dois lados. Funciona (as funcoes
# nao tem estado), mas depois que os fontes do modelo sairam para models/, isso
# viraria DUAS COPIAS DO ARQUIVO em duas arvores -- que divergem em silencio.
# Uma .so a mais no SDK e mais barata que essa divergencia.

xtrack_inc_dir = include_directories('..')

xtrack_lib = shared_library('xtrack',
    files('TrackQuery.cpp'),
    include_directories : xtrack_inc_dir,
    dependencies        : [mixr_dep],
    install             : true,
    install_dir         : get_option('libdir'),
    install_tag         : 'sdk',
    build_rpath         : mixr_libdir,
    install_rpath       : mixr_libdir,
)

xtrack_dep = declare_dependency(
    link_with           : xtrack_lib,
    include_directories : xtrack_inc_dir,
    dependencies        : [mixr_dep],
)
