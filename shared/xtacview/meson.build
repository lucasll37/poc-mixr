# Biblioteca compartilhada entre os subprojetos poc/ que exportam telemetria
# para o Tacview. Segue o padrao 'shared/x<nome>' das bibliotecas de extensao
# dos exemplos MIXR (factory propria + classes no namespace mixr::x<nome>).
#
# O DataRecord.pb.h fica na RAIZ do include do pacote mixr (nao em
# mixr/recorder/), por isso o protobuf entra como dependencia explicita.

xtacview_inc_dir = include_directories('..')

xtacview_lib = static_library('xtacview',
    files(
        'RealtimeTelemetryServer.cpp',
        'TacviewOutput.cpp',
        'factory.cpp',
    ),
    include_directories : xtacview_inc_dir,
    dependencies        : [mixr_dep, protobuf_dep],
)

xtacview_dep = declare_dependency(
    link_with           : xtacview_lib,
    include_directories : xtacview_inc_dir,
    dependencies        : [mixr_dep, protobuf_dep],
)
