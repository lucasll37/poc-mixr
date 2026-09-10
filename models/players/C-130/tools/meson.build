# ==============================================================================
# dump-tree-model -- gerador do <TreeNodesModel> para o Groot (ver
# ../docs/ARCHITECTURE.md e CLAUDE.md raiz, secao "Groot").
#
# Reusa 'bt_sources'/'domain_sources' (definidos no meson.build do projeto, um
# nivel acima) -- os MESMOS fontes que tests/meson.build compila em
# 'test-tree', entao herda a mesma propriedade: linka BehaviorTree.CPP, NAO
# linka MIXR. E' o que permite gerar o manifesto sem levantar Station nenhuma.
#
# Fica FORA do gate '-Dtests' (o subdir('tools') do meson.build do projeto e'
# incondicional) de proposito: nao e' teste, e' ferramenta de desenvolvimento
# -- os alvos 'create-bt'/'update-bt' do Makefile dependem dela.
# ==============================================================================
dump_tree_model = executable('dump-tree-model',
    files('dump_tree_model.cpp') + bt_sources + domain_sources,
    link_args           : rpath_link_args,
    include_directories : model_inc,
    dependencies        : [behavior_tree_dep],
    install             : false,
)
