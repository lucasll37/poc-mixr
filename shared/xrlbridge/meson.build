# A ponte de comando/observacao entre o host de RL (src/rl/bindings) e
# RLBridgeBehavior (models/flight). Mesmo motivo estrutural de shared/xboard
# para ser shared_library() em vez de estatica -- ver o cabecalho de
# RLBridge.hpp: escrita/leitura cruzam a fronteira host (executavel) / modelo
# (.so aberto com dlopen), e uma lib estatica daria a cada lado a sua propria
# copia dos mapas.
#
# Instalada, pelo mesmo motivo de xboard: o consumidor em dist/bin/ (ou, aqui,
# o modulo Python em dist/python/) precisa acha-la em dist/lib/.

xrlbridge_inc_dir = include_directories('..')

xrlbridge_lib = shared_library('xrlbridge',
    files('RLBridge.cpp'),
    include_directories : xrlbridge_inc_dir,
    install             : true,
    install_dir         : get_option('libdir'),
    install_tag         : 'sdk',
)

xrlbridge_dep = declare_dependency(
    link_with           : xrlbridge_lib,
    include_directories : xrlbridge_inc_dir,
)
