# ==============================================================================
# Suite deste modelo -- duas camadas: a regra pura (domain, a lei de
# navegacao proporcional + a espoleta de proximidade) e a FORMA do artefato
# (contract). Nao ha suite 'tree' -- este modelo nao tem arvore de
# comportamento (guiagem e controle continuo, nao decisao discreta, ver
# docs/ARCHITECTURE.md) -- nem 'native' (levantar uma Station so' pra
# instanciar um Missile isolado nao provaria nada que 'domain' + o cenario
# sandbox/A4-6DOF-MISSILE ja nao provem).
#
# 'model_inc' e 'missile_lib' vem do escopo do meson.build PAI (Meson
# compartilha o namespace entre um project() e os subdir() dele) -- nao sao
# redeclarados aqui.
#
# ARMADILHA MEDIDA (documentada em CLAUDE.md, raiz, e em todo meson.build de
# modelo deste repositorio): 'meson test' devolve rc=0 para suite VAZIA
# ("No tests defined."), e o default de -Dtests e false. O Makefile confere
# a contagem com 'meson introspect --tests' antes de rodar -- ver o alvo
# 'test' dele.
# ==============================================================================
gtest_dep      = dependency('gtest', method: 'pkg-config')
gtest_main_dep = dependency('gtest_main', method: 'pkg-config')

# Camada 1 -- a regra pura: sem MIXR, sem rpath nenhum. Acrescente um
# 'test_XXX.cpp' aqui ao lado a cada regra nova em domain/. 'domain_sources'
# vem do meson.build PAI -- os MESMOS arquivos que o .so compila, nunca uma
# copia.
test_domain = executable('test-domain',
    files('domain/test_Guidance.cpp') + domain_sources,
    include_directories : model_inc,
    dependencies        : [gtest_dep, gtest_main_dep],
    install             : false,
)
test('domain', test_domain, suite: 'domain', timeout: 60)

# Camada 2 -- a FORMA do artefato (nao o comportamento): exatamente um
# simbolo T exportado (o ponto de entrada de MIXR_PLUGIN_DEFINE) e nenhuma
# dependencia dinamica nao resolvida -- os dois criterios que
# CONTRIBUTING.md secao 5.1 (raiz) ja documentava a mao.
check_contract = find_program('check_contract.sh')
test('contrato-simbolo-unico-e-deps-resolvidas', check_contract,
    args    : [missile_lib],
    suite   : 'contract',
    timeout : 30,
)
