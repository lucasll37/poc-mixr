# Inferencia ONNX -- o motor, contido numa .so so.
#
# Terceira shared_library() do SDK pelo mesmo motivo estrutural de xboard e
# xrlbridge (ver o cabecalho de Infer.hpp), com um agravante proprio: o ONNX
# Runtime sao ~1,9 GB de .a em Debug, e models/flight gera QUATRO artefatos do
# mesmo model_sources. Dentro do plugin seriam quatro copias; aqui e uma.
#
# O onnxruntime e declarado AQUI, e nao no meson.build raiz, porque este e o
# UNICO consumidor -- mesmo precedente do ftxui em app/src/meson.build e do
# nlohmann_json que src/server declarava.

onnx_dep = dependency('onnxruntime', method: 'pkg-config', required: true)

xinfer_inc_dir = include_directories('..')

# DUAS flags carregam o desenho inteiro:
#
#   --exclude-libs,ALL  esconde os simbolos que vem dos .a do ORT. Sem ela,
#                       gnu_symbol_visibility:'hidden' NAO basta -- ela nao se
#                       aplica a objetos vindos de arquivo estatico (a mesma
#                       razao pela qual models/flight/meson.build precisa dela
#                       para a BehaviorTree.CPP). Medido: com a flag, o .so
#                       exporta so a API desta lib.
#
#   --strip-debug       derruba os 537 MB de debug info que o ORT em Debug
#                       carrega (576 MB -> ~39 MB). E codigo de TERCEIRO, que
#                       ninguem aqui vai depurar; o codigo proprio desta lib
#                       sao 150 linhas.
#
# '-s' (strip TUDO) e nao '--strip-debug': medido, --strip-debug preserva a
# .symtab e para em 82 MB; -s chega aos ~39 MB. A API publica continua
# visivel porque ela vive na .dynsym, que o -s nao toca -- conferido com
# 'nm -D' depois.
xinfer_link_args = ['-Wl,--exclude-libs,ALL', '-Wl,-s'] + rpath_link_args

xinfer_lib = shared_library('xinfer',
    files('Infer.cpp'),
    include_directories   : xinfer_inc_dir,
    dependencies          : [onnx_dep, xlog_dep],
    link_args             : xinfer_link_args,
    gnu_symbol_visibility : 'hidden',
    install               : true,
    install_dir           : get_option('libdir'),
    install_tag           : 'sdk',
    build_rpath           : mixr_libdir,
    # '$ORIGIN' porque esta lib depende de libxlog.so, que fica NO MESMO
    # diretorio (dist/lib) -- sem isto, 'ldd' acusa "libxlog.so => not found"
    # assim que ela e carregada de fora do build. As outras libs de shared/
    # nao precisam: nenhuma delas depende de outra irma.
    install_rpath         : mixr_libdir + ':$ORIGIN',
)

# SEM 'dependencies: [onnx_dep]' aqui, de proposito: e isto que mantem o ONNX
# Runtime PRIVADO. Quem consome esta lib (o plugin do modelo) linka so
# '-lxinfer' e nunca ve header nem biblioteca do ORT -- medido, o NEEDED do
# plugin fica so libxinfer.so. Acrescentar onnx_dep aqui vazaria o ORT para
# todo consumidor e obrigaria o poc-mixr-sdk.pc a declarar 'Requires:
# onnxruntime'.
#
# xlog_dep FICA porque o consumidor pode querer logar, e ele ja e parte do SDK.
xinfer_dep = declare_dependency(
    link_with           : xinfer_lib,
    include_directories : xinfer_inc_dir,
)
