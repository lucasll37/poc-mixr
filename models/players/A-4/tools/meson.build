# ==============================================================================
# dump-tree-model -- gerador do <TreeNodesModel> para o Groot (CLAUDE.md,
# secao "Groot -- editor e monitor ao vivo").
#
# Reusa bt_sources/bt_sdk_sources/domain_sources (definidos no meson.build do
# projeto, um nivel acima) -- os MESMOS fontes que test-tree/test-native ja
# compilam em tests/meson.build -- entao herda a mesma propriedade de
# test-tree: linka BehaviorTree.CPP + SDK, nao levanta MIXR sozinho.
#
# Fica FORA de tests/ (e do gate '-Dtests', ver o 'subdir(tools)'
# incondicional no meson.build do projeto) de proposito: nao e teste, e
# ferramenta de desenvolvimento -- os alvos 'create-bt'/'update-bt' do
# Makefile dependem dela (os dois via tools/update_bt_models.py), e o teste
# 'tree-model-sync' (tests/meson.build) passa o objeto abaixo direto.
# ==============================================================================
dump_tree_model = executable('dump-tree-model',
    files('dump_tree_model.cpp') + bt_sources + bt_sdk_sources + domain_sources,
    link_args           : ['-Wl,--disable-new-dtags'],
    include_directories : model_inc,
    dependencies        : [behavior_tree_dep, sdk_dep],
    install             : false,
    build_rpath         : mixr_libdir + ':' + sdk_libdir,
)
