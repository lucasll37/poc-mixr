# O contrato de eventos que atravessam fronteira de PLUGIN -- payload
# (base::Object-derivada, uma pasta payloads/<TOKEN>/ por evento, nomeada
# igual ao token) + token (EventTokens.hpp, ledger UNICO -- ver o "porque"
# no cabecalho dele).
#
# Mora aqui, na RAIZ, e nao em shared/x<nome>/ como as outras seis
# shared_library() de fronteira de plugin (xboard/xlog/xtrack/xrlbridge/
# xinfer/xpyembed) -- excecao deliberada: evento e o eixo central da
# modelagem deste projeto (ver events/README.md), entao ganhou pasta propria
# em destaque na raiz do repositorio. O MECANISMO nao muda em nada: e
# shared_library() de verdade, e nao static, pelo MESMO motivo de
# shared/xboard -- um dynamic_cast de um payload recebido em OUTRO .so
# (dlopen) so e seguro se a classe do payload vier de uma lib linkada por
# ambos os lados, do contrario cada plugin enxergaria seu proprio type_info
# para o "mesmo" tipo. Ver o cabecalho de payloads/EID_ALERT/TacticalAlert.hpp.
#
# Instalada, porque qualquer plugin de modelo (e o host, se algum dia
# precisar) precisa acha-la em dist/lib/ -- mesmo raciocinio de xboard.

events_inc_dir = include_directories('..')

# Ao contrario de xboard/xrlbridge (structs/mapas simples, sem MIXR), o
# payload aqui e um mixr::base::Object de verdade (DECLARE_SUBCLASS,
# ref-contagem, RTTI proprio do framework) -- precisa de mixr_dep.
events_lib = shared_library('events',
    files('payloads/EID_ALERT/TacticalAlert.cpp'),
    include_directories : events_inc_dir,
    dependencies        : mixr_dep,
    install             : true,
    install_dir         : get_option('libdir'),
    install_tag         : 'sdk',
)

events_dep = declare_dependency(
    link_with           : events_lib,
    include_directories : events_inc_dir,
    dependencies        : mixr_dep,
)
