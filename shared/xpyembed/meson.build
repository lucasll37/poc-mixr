# Python embarcado -- rodar um script de decisao de dentro do frame.
#
# NAO linka libpython: ela e alcancada por dlopen(RTLD_GLOBAL) em runtime. As
# tres razoes (todas medidas) estao no cabecalho de PyEmbed.hpp; a mais dura e
# que o /usr/bin/python3 do Ubuntu tem a libpython ESTATICA, entao um
# DT_NEEDED daria um SEGUNDO runtime no processo do src/rl.
#
# O efeito colateral bom: nenhum meson.build nem main.cpp de host precisou
# mudar, e o Python virou dependencia OPCIONAL de runtime.

xpyembed_inc_dir = include_directories('..')

xpyembed_lib = shared_library('xpyembed',
    files('PyEmbed.cpp'),
    include_directories   : xpyembed_inc_dir,
    dependencies          : [xlog_dep, dl_dep],
    gnu_symbol_visibility : 'hidden',
    install               : true,
    install_dir           : get_option('libdir'),
    install_tag           : 'sdk',
    build_rpath           : mixr_libdir,
    # '$ORIGIN' pelo mesmo motivo de xinfer: depende de libxlog.so, que fica
    # no MESMO diretorio.
    install_rpath         : mixr_libdir + ':$ORIGIN',
)

xpyembed_dep = declare_dependency(
    link_with           : xpyembed_lib,
    include_directories : xpyembed_inc_dir,
)
