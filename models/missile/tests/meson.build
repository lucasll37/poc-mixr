# ==============================================================================
# Suite 'domain' do missile -- so a lei de guiagem (domain::pursuit()), pura:
# sem MIXR, sem JSBSim, sem Station. Mesma ideia da camada domain/ do flight
# (models/flight/tests/meson.build) -- so muito menor, porque este modelo tem
# uma classe so.
# ==============================================================================
gtest_dep      = dependency('gtest', method: 'pkg-config')
gtest_main_dep = dependency('gtest_main', method: 'pkg-config')

test_domain = executable('test-domain',
    files('domain/test_Guidance.cpp', '../src/domain/Guidance.cpp'),
    include_directories : model_inc,
    dependencies         : [gtest_dep, gtest_main_dep],
    install              : false,
)
test('domain', test_domain, suite: 'domain', timeout: 60)
