# ==============================================================================
# Suite deste template -- duas camadas, no mesmo espirito de
# models/players/missile/tests/meson.build (um modelo pequeno nao precisa da
# suite de 3 camadas de models/players/A-4/tests/meson.build).
#
# 'model_inc' e 'template_lib' vem do escopo do meson.build PAI (Meson
# compartilha o namespace entre um project() e os subdir() dele) -- nao sao
# redeclarados aqui.
#
# ARMADILHA MEDIDA (documentada em models/README.md e em todo meson.build de
# modelo deste repositorio): 'meson test' devolve rc=0 para suite VAZIA
# ("No tests defined."), e o default de -Dtests e false. O Makefile confere
# a contagem com 'meson introspect --tests' antes de rodar -- ver o alvo
# 'test' dele.
# ==============================================================================
gtest_dep      = dependency('gtest', method: 'pkg-config')
gtest_main_dep = dependency('gtest_main', method: 'pkg-config')

# Camada 1 -- a regra pura: sem MIXR, sem rpath nenhum. Acrescente um
# 'test_XXX.cpp' aqui ao lado a cada regra nova em domain/.
test_domain = executable('test-domain',
    files(
        'domain/test_ExampleThreshold.cpp',
        '../src/domain/ExampleThreshold.cpp',
    ),
    include_directories : model_inc,
    dependencies        : [gtest_dep, gtest_main_dep],
    install             : false,
)
test('domain', test_domain, suite: 'domain', timeout: 60)

# Camada 2 -- a FORMA do artefato (nao o comportamento): exatamente um
# simbolo T exportado (o ponto de entrada de MIXR_PLUGIN_DEFINE) e nenhuma
# dependencia dinamica nao resolvida. Mesmo script de
# models/players/fixtures/stub/tests/check_contract.sh -- os dois criterios que
# models/README.md secao 2.3 ja documentava a mao.
check_contract = find_program('check_contract.sh')
test('contrato-simbolo-unico-e-deps-resolvidas', check_contract,
    args    : [template_lib],
    suite   : 'contract',
    timeout : 30,
)
