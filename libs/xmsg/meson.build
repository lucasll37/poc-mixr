# Sistema de mensagens configuraveis por EDL.
#
# rules/ e livre de MIXR de proposito -- e o que permite testar a logica de
# deteccao (limiar com histerese, deadband, derivada, piso de emissao) no alvo
# test-domain, sem levantar simulacao nenhuma. O resto da lib e a fronteira
# com o framework.
xmsg_inc_dir = include_directories('..')

xmsg_lib = static_library('xmsg',
    files(
        'rules/Deadband.cpp',
        'rules/EmitGate.cpp',
        'rules/RateWindow.cpp',
        'rules/Schmitt.cpp',

        'FieldCatalog.cpp',
        'SnapshotSource.cpp',
        'SlotUnits.cpp',

        'Condition.cpp',
        'MsgThreshold.cpp',
        'MsgChanged.cpp',
        'MsgRate.cpp',

        'RecordWriter.cpp',
        'MsgSink.cpp',
        'MsgFileSink.cpp',
        'MsgReport.cpp',
        'MsgFeed.cpp',
        'factory.cpp',
    ),
    include_directories : xmsg_inc_dir,
    dependencies        : [mixr_dep, xlog_dep],
)

xmsg_dep = declare_dependency(
    link_with           : xmsg_lib,
    include_directories : xmsg_inc_dir,
    dependencies        : [mixr_dep, xlog_dep],
)
