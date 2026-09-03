# O wrapper Gymnasium -- ver rl/README.md.
#
# Analogo a ./app: nao e mais uma poc de src/poc/ (nao varia "onde a decisao
# roda", nao troca DIS com ninguem) e nem sequer produz um EXECUTAVEL -- gera
# um modulo de extensao Python (rl/bindings/) mais o pacote Python puro
# (rl/python/mixr_gym/) que o consome.
#
# Onde os dois pousam em dist/, para 'import mixr_gym' achar a extensao
# compilada ao lado dos .py: dist/python/mixr_gym/{__init__.py, env.py,
# _native*.so}.
rl_python_pkg_dir = get_option('prefix') / 'python' / 'mixr_gym'

subdir('./bindings')

install_subdir('python/mixr_gym',
    install_dir : get_option('prefix') / 'python',
    exclude_directories : ['__pycache__'],
)
