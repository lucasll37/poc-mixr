# Biblioteca compartilhada entre os subprojetos de src/ que exportam telemetria
# para o Tacview. Segue o padrao 'shared/x<nome>' das bibliotecas de extensao
# dos exemplos MIXR (factory propria + classes no namespace mixr::x<nome>).
#
# O DataRecord.pb.h fica na RAIZ do include do pacote mixr (nao em
# mixr/recorder/), por isso o protobuf entra como dependencia explicita.
#
# xlog_dep: RealtimeTelemetryServer.cpp escreve via LOG(...), nao direto em
# std::cout/std::cerr -- e o motivo e o mesmo do LOG() do modelo: o ./app e
# dono do terminal (FTXUI fullscreen) e uma linha crua no meio do desenho
# fica piscando por cima da UI. Passando por xlog, xlog::setConsoleEnabled
# (chamado pelo ./app antes do Fullscreen()) silencia isso igual a qualquer
# outro LOG(...) -- sem essa dependencia explicita o link so funcionava por
# xlog_dep chegar transitivamente de quem consome xtacview_dep.

xtacview_inc_dir = include_directories('..')

xtacview_lib = static_library('xtacview',
    files(
        'RealtimeTelemetryServer.cpp',
        'TacviewOutput.cpp',
        'ExposedDataRecorder.cpp',
        'factory.cpp',
    ),
    include_directories : xtacview_inc_dir,
    dependencies        : [mixr_dep, protobuf_dep, xlog_dep],
)

xtacview_dep = declare_dependency(
    link_with           : xtacview_lib,
    include_directories : xtacview_inc_dir,
    dependencies        : [mixr_dep, protobuf_dep, xlog_dep],
)
