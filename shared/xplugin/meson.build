# Carga de modelos MIXR em tempo de execucao (dlopen) -- mesmo padrao
# 'shared/x<nome>' de xtacview/xclock/xjoystick/xlog/xmsg, com UMA diferenca
# que e o coracao do desenho: esta biblioteca exporta DOIS dependency.
#
#   xplugin_abi_dep   O CONTRATO. Header-only -- nao ha nada para linkar.
#                     E o unico que um PLUGIN usa.
#   xplugin_dep       O REGISTRO + as classes EDL ( PluginLoader ) e
#                     ( PluginModule ). static_library, como todo shared/x*.
#                     So entra nos EXECUTAVEIS, nunca num plugin.
#
# POR QUE DOIS. Todas as libs de shared/ sao static_library(). Se um plugin
# .so linkasse xplugin_lib, ele ganharia a SUA PROPRIA copia do registro: a
# aplicacao registraria de um lado e consultaria do outro. E a armadilha
# registrada em contexts/BTCPP-CONTEXT.md:7262-7270. Por isso PluginAbi.hpp
# nao tem uma linha de codigo -- so declaracoes, macros e constantes.

xplugin_inc_dir = include_directories('..')

# Impressao digital do build, gravada NOS DOIS LADOS e comparada na carga.
#
# E a unica coisa que detecta um plugin compilado contra outro pacote do MIXR:
# as .so do fork nao tem SONAME versionado (libmixr_base.so, sem .so.1.0.5),
# entao o loader nao percebe a troca sozinho. Divergencia aqui e AVISO, nao
# recusa -- ver a secao "Limites" do README.md desta pasta para o que isso
# alcanca e o que nao alcanca.
xplugin_version_args = [
    '-DMIXR_PLUGIN_PKG_VERSION="' + mixr_dep.version() + '"',
    '-DMIXR_PLUGIN_BUILD_ID="' + cpp.get_id() + ' ' + cpp.version() + '"',
]

# Para o PLUGIN.
xplugin_abi_dep = declare_dependency(
    include_directories : xplugin_inc_dir,
    compile_args        : xplugin_version_args,
    dependencies        : [mixr_dep],
)

# Para o HOST.
xplugin_lib = static_library('xplugin',
    files(
        'PluginRegistry.cpp',
        'PluginModule.cpp',
        'PluginLoader.cpp',
        'factory.cpp',
    ),
    cpp_args            : xplugin_version_args,
    include_directories : xplugin_inc_dir,
    dependencies        : [mixr_dep, dl_dep],
)

xplugin_dep = declare_dependency(
    link_with           : xplugin_lib,
    include_directories : xplugin_inc_dir,
    compile_args        : xplugin_version_args,
    # dl_dep viaja junto: o bandit-dis nao usa behavior_tree_dep e portanto
    # nao herdaria '-ldl' de nenhum .pc (conferido: o .pc da
    # behaviortree.cpp.asa traz so -lbehaviortree_cpp_v3 -lpthread).
    dependencies        : [mixr_dep, dl_dep],
)
